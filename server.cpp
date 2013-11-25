/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
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

using namespace std; 

static int current_message_id = 0;

void error(string msg)
{
    cout << msg;
    exit(1);
}

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
    // Hashmap of stacks (WHOA)
    unordered_map <string, queue<Message> > messages;

    int portno = 9009;
    int sockfd, newsockfd, clilen;
    char buffer[256], output_buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    // Bind socket
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    unsigned length_ptr = 16;

    while (1) {
        bzero(buffer,256);
        int readSocket = recvfrom(sockfd, &buffer, 255, 0, (struct sockaddr *) &cli_addr, &length_ptr);
        printf("Incoming Message\n");
        char ip_source[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(cli_addr.sin_addr), ip_source, INET_ADDRSTRLEN);
        
        printf("From: %s\n", ip_source);
        printf("Data: %s\n", buffer);
        string message;
        string dest;

        switch (buffer[1]) {
            case '0':
                printf("JOIN received.\n");
                break;

            case '1':
                if (messages[ip_source].size() < 1) {
                    bzero(output_buffer, 256);
                    output_buffer[0] = buffer[0];
                    output_buffer[1] = '3'; // ACK means that there are no more messages to give
                    sendto (sockfd, output_buffer, 255, 0, (struct sockaddr *) &cli_addr, length_ptr);
                } else {
                    bzero(output_buffer, 256);
                    output_buffer[0] = buffer[0];
                    output_buffer[1] = '2'; // we're issuing a SEND back to the client;
                    Message temp_msg = messages[ip_source].front();
                    sprintf(output_buffer+2, "%s", temp_msg.source.c_str());
                    sprintf(output_buffer+18, "%s", temp_msg.message.c_str());
                    sendto (sockfd, output_buffer, 255, 0, (struct sockaddr *) &cli_addr, length_ptr);
                    bzero(buffer, 256);
                    recvfrom(sockfd, &buffer, 255, 0, (struct sockaddr *) &cli_addr, &length_ptr);

                    if (buffer[1] == '3') {
                        printf("ACK RECEIVED. POPPING.");
                        messages[ip_source].pop();
                    }
                }
                printf("GET received\n");
                break;

            case '2':
                dest = string(buffer+2);
                message = string(buffer+18);

                messages[dest].push(Message(ip_source, message));
                cout << " Message " << message << "/message" << "\n";
                cout << " Dest " << dest <<  "/dest\n";
                printf("SEND received\n");
                bzero(buffer+1, 255);
                buffer[1] = '3';
                sendto (sockfd, buffer, 255, 0, (struct sockaddr *) &cli_addr, length_ptr);
                break;

            case '3':
                printf("ACK received\n");
                break;

            default:
                printf("Noise received.\n");
                sendto (sockfd, "Server has received your datagram.", 255, 0, (struct sockaddr *) &cli_addr, length_ptr);
                break;
        }
     }

     sendto (sockfd, "Server has received your datagram.", 255, 0, (struct sockaddr *) &cli_addr, length_ptr);

     return 0; 
}
