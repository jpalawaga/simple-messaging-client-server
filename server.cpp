#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <unordered_map>
#include <queue>
#include <thread>         // std::thread, std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds

using namespace std; 

int current_message_id = 0;
const int FIELD_SEQ = 0;
const int FIELD_TYPE = 1;
const char RESPONSE_GET     = '1';
const char RESPONSE_SEND    = '2';
const char RESPONSE_ACK     = '3';
const char RESPONSE_RECEIPT = '4';
const int BUFFER_SZ = 256;
const int server_port = 9009;
const int client_port = 9010;

class Message
{
    public:
        string source;
        string message;
        string time;
        int message_id;
        bool read;

    Message() {

    }

    Message (string s, string m) {
        source = s;
        message = m;
        message_id = current_message_id++;
        read = false;
    }
};

int main(int argc, char *argv[])
{
    // Hashmap of queues (WHOA)
    unordered_map <string, queue<Message> > messages;

    // Networking variables
    int sockfd, newsockfd, clilen;
    struct sockaddr_in serv_addr, cli_addr, rec_addr;
    int n, retryCounter = 0;
    bool recvfail = false;

    // Output buffer is only here for clarity. Could implement easily with 1 buffer.
    char buffer[BUFFER_SZ], output_buffer[BUFFER_SZ];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        cout << "ERROR opening socket";
    }
    memset((char *) &serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(server_port);

    // Bind socket
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        cout << "ERROR binding";

    // Testing, used to be 16.
    unsigned length_ptr;

    while (1) {
        memset(buffer, 0, BUFFER_SZ);
        int readSocket = recvfrom(sockfd, &buffer, BUFFER_SZ, 0, (struct sockaddr *) &cli_addr, &length_ptr);
        char ip_source[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(cli_addr.sin_addr), ip_source, INET_ADDRSTRLEN);
        
        printf("\n[%s] Incoming Message\n", ip_source);
        printf("  Data: %s\n", buffer);
        string message;
        string dest;
        unsigned short port = cli_addr.sin_port;
        Message temp_msg;

        switch (buffer[1]) {
            case '0':
                printf("    JOIN received\n");
                memset(buffer, 0, BUFFER_SZ);
                buffer[FIELD_SEQ] = '0';
                buffer[FIELD_TYPE] = RESPONSE_ACK;
                sendto (sockfd, buffer, BUFFER_SZ, 0, (struct sockaddr *) &cli_addr, length_ptr);
                break;

            case '1':
                printf("    GET received\n");

                if (messages[ip_source].size() < 1) {

                    // No messages to send, reply w/ ACK to signal this.
                    memset(output_buffer, 0, BUFFER_SZ);
                    output_buffer[FIELD_SEQ] = buffer[FIELD_SEQ];
                    output_buffer[FIELD_TYPE] = RESPONSE_ACK; // ACK means that there are no more messages to give
                    sendto(sockfd, output_buffer, BUFFER_SZ, 0, (struct sockaddr *) &cli_addr, length_ptr);

                } else {

                    // Send a message. First prepare 
                    memset(output_buffer, 0, BUFFER_SZ);
                    output_buffer[FIELD_SEQ] = buffer[FIELD_SEQ];
                    output_buffer[FIELD_TYPE] = RESPONSE_SEND;
                    Message temp_msg = messages[ip_source].front();
                    sprintf(output_buffer+2, "%s", temp_msg.source.c_str());
                    sprintf(output_buffer+18, "%s", temp_msg.message.c_str());

                    // Prepare buffer for receive
                    memset(buffer, 0, BUFFER_SZ);

                    // Redundant ACK sending.
                    recvfail = true;
                    retryCounter = 0;
                    while (recvfail) {
                        sendto(sockfd, output_buffer, BUFFER_SZ, 0, (struct sockaddr *) &cli_addr, length_ptr);

                        if (recvfrom(sockfd, &buffer, BUFFER_SZ, 0, (struct sockaddr *) &cli_addr, &length_ptr) > 0) {
                            recvfail = false;
                        } else {
                            retryCounter++;
                        }

                        if (retryCounter > 3) {
                            printf("    Message delivery failed after 3 tries (client disappeared?)!\n");
                            break;
                        }
                    }

                    // Send Delivery Receipt
                    if (buffer[FIELD_TYPE] == RESPONSE_ACK) {
                        printf("    Message delivered successfully.\n");
                        messages[ip_source].pop();

                        output_buffer[FIELD_SEQ] = '0';  // Receipts have no sequence number
                        output_buffer[FIELD_TYPE] = '4'; // Receipts have type "Receipt"
                        memset(output_buffer+2, 0, BUFFER_SZ-2);
                        sprintf(output_buffer+18,"%d",temp_msg.message_id);

                        memset(&rec_addr, 0, sizeof rec_addr);
                        rec_addr.sin_family = AF_INET;
                        inet_pton(AF_INET, temp_msg.source.c_str(), &rec_addr);
                        rec_addr.sin_port = htons(client_port);
                        sendto(sockfd, output_buffer, BUFFER_SZ, 0, (struct sockaddr *) &rec_addr, length_ptr);
                    }
                }
                break;

            case '2':
                printf("  SEND received\n");

                // Parse message incoming from client, push on stack
                dest = string(buffer+2);
                message = string(buffer+18);
                temp_msg = Message(ip_source, message);
                messages[dest].push(temp_msg);

                cout << "    [DST]" << dest    << "\n";
                cout << "    [MSG]" << message << "\n";
                buffer[FIELD_TYPE] = RESPONSE_ACK;

                // Send ACK.
                memset(buffer+2, 0, BUFFER_SZ-2);
                sprintf(buffer+18,"%d",temp_msg.message_id);
                sendto (sockfd, buffer, BUFFER_SZ, 0, (struct sockaddr *) &cli_addr, length_ptr);
                break;

            default:
                printf("  Noise received.\n");
                break;
        }
     }

     return 0; 
}
