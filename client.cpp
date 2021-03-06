#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <netinet/in.h>
#include <unistd.h>
#include <thread>         // std::thread, std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds
#include "WindowManager.h"

using namespace std;

const int FIELD_SEQ = 0;
const int FIELD_TYPE = 1;
const char RESPONSE_GET     = '1';
const char RESPONSE_SEND    = '2';
const char RESPONSE_ACK     = '3';
const char RESPONSE_RECEIPT = '4';
const int BUFF_SZ = 256;

void error(string msg)
{
    cout << msg;
    exit(0);
}

void checkForReadReceipts(WindowManager * winMan, int sockfd) {

    struct sockaddr_in serv_addr, cli_addr;
    unsigned length_ptr = 16;
    char buffer[BUFF_SZ];

    while (1) {
        // We only want to process read receipts here.
        int readSocket = recvfrom(sockfd, &buffer, 255, MSG_DONTWAIT | MSG_PEEK, (struct sockaddr *) &cli_addr, &length_ptr);
        if (readSocket > 0 && buffer[FIELD_TYPE] == RESPONSE_RECEIPT) {
            int readSocket = recvfrom(sockfd, &buffer, 255, MSG_DONTWAIT, (struct sockaddr *) &cli_addr, &length_ptr);
            winMan->write("Message ID #"+string(buffer+18)+" has been delivered!");
            memset(buffer, 0, BUFF_SZ);
        } 
    }
}

int main(int argc, char *argv[])
{
    // Window Manager
    WindowManager winMan = WindowManager();

    // Networking variables
    int portno = 9009;
    int sockfd, n;
    struct sockaddr_in serv_addr, serv_addr2;
    struct hostent *server;
    char buffer[BUFF_SZ], temp_buffer[BUFF_SZ];
    int current_sequence_number = 0;

    // Variables for resend attempts
    int retryCounter = 0;
    bool recvfail = false;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    thread messageReciever(checkForReadReceipts, &winMan, sockfd);
    messageReciever.detach();

    serv_addr2.sin_family = AF_INET;
    serv_addr2.sin_addr.s_addr = INADDR_ANY;
    serv_addr2.sin_port = htons(9010);
    if (::bind(sockfd, (struct sockaddr *) &serv_addr2, sizeof(serv_addr2)) < 0)
        error("ERROR on binding");

    // Check if we can find socket.
    if (sockfd < 0) {
        winMan.end();
        cout << "ERROR: Socket could not be opened.";
        exit(0);
    }

    server = gethostbyname(argv[1]);

    // Try to resolve host
    if (server == NULL) {
        winMan.end();
        cout << "ERROR: No such host.";
        exit(0);
    }

    // Setup default connection.
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
        winMan.end();
        cout << "ERROR: Connection error.";
        exit(0);
    }

    // Send JOIN message, check server availility
    buffer[FIELD_SEQ]  = '0';
    buffer[FIELD_TYPE] = '0';
    n = write(sockfd,buffer, BUFF_SZ);
    if (n < 0) {
        winMan.end();
        cout << "ERROR: Could not write to socket.";
        exit(0);
    }
    memset(buffer, 0, BUFF_SZ);

    struct timeval timeout;      
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        error("setsockopt failed\n");
    }

    // We're just looking to make sure the server is up.
    // The server replies with a ACK, which could contain session information.
    n = read(sockfd, buffer, BUFF_SZ);
    if (n < 0) {
        winMan.end();
        cout << "ERROR: Connection error, JOIN not responded.\n";
        exit(0);
    }

    while(1) {

        // Get destination IP address
        char selection[1];
        winMan.requestInput("[R]eceive | [S]end | e[X]it $> ", selection, 1);

        char buffer2[100];
        memset(buffer, 0, BUFF_SZ);
        
        switch(selection[0]) {
            case 'r':
            case 'R':

                // We will look until we receive every message.
                while(1) {
                    buffer[FIELD_SEQ] = 49 + (current_sequence_number++ % 10);
                    buffer[FIELD_TYPE] = RESPONSE_GET;  // GET action

                    // Don't let MessageReceiver intercept messages
                    n = write(sockfd, buffer, BUFF_SZ);
                    n = read(sockfd, buffer, BUFF_SZ);

                    if (buffer[FIELD_TYPE] == RESPONSE_ACK) {
                        winMan.write("No more messages to retreive!");
                        break;
                    }

                    if (buffer[FIELD_TYPE] == RESPONSE_RECEIPT) {
                        winMan.write("Message from self received.");
                        continue;
                    }

                    winMan.write("");
                    sprintf(buffer2, "============ FROM : %s =============",buffer+2);
                    winMan.write(buffer2);
                    winMan.write(buffer+18);
                    winMan.write("==============================================");
                    winMan.write("");

                    memset(buffer+2, 0, BUFF_SZ-2);
                    buffer[FIELD_TYPE] = RESPONSE_ACK;
                    n = write(sockfd, buffer, BUFF_SZ);

                    // Allow the chance for a delivery receipt to arrive.
                    // @TODO: needs moar design.
                    std::this_thread::sleep_for (std::chrono::milliseconds(100));
                }
                break;

            case 's':
            case 'S':
                buffer[FIELD_SEQ] = 49 + (current_sequence_number++ % 10);
                buffer[FIELD_TYPE] = RESPONSE_SEND;

                // Get destination IP address / Message
                winMan.requestInput("Please enter the recipient's IP: ", buffer+2, 16);
                winMan.requestInput("msg $> ", buffer+18, BUFF_SZ-18);

                    // Attempt 3x to deliver a message to the server.
                    recvfail = true;
                    retryCounter = 0;
                    while (recvfail) {
                        if (write(sockfd,buffer, 256) < 1) {
                            error("ERROR writing to socket");
                        }

                        memset(temp_buffer, 0, BUFF_SZ);
                        if (read(sockfd, temp_buffer, BUFF_SZ) < 1) {
                            retryCounter++;
                            continue;
                        }

                        if (retryCounter > 3) {
                            winMan.write("    Message delivery failed after 3 tries (server disappeared?)!\n");
                            break;
                        }

                        if (((int)(temp_buffer[FIELD_SEQ]) - 48) == current_sequence_number && temp_buffer[FIELD_TYPE] == RESPONSE_ACK) {
                            winMan.write("ACK successfully recieved. Message ID: "+string(temp_buffer+18));
                        } else {
                            winMan.write("Server error occured...");
                        }
                        break;
                    }

                break;

                case 'x':
                case 'X':
                case 'e':
                case 'E':
                    winMan.end();
                    return 0;
            default:
                winMan.write("invalid option!");
                break;
        }
    }


    return 0;
}
