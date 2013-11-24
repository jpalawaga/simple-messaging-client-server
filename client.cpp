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
#include <curses.h>

using namespace std;

static const char ACK_RESPONSE = '3';

// Message format:
// +---+------+---------
// | 0 | 1-16 | message...
// +---+------+---------
//  seq  dest
//
// 0=JOIN, 1=GET, 2=SEND, 3=ACK

void error(string msg)
{
    cout << msg;
    exit(0);
}


static int current_sequence_number = 1;
static char buffer[256];

int main(int argc, char *argv[])
{
    int portno = 9009;
    int sockfd, n;

    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    n = write(sockfd,"000",strlen(buffer));
    n = read(sockfd,buffer,255);
    if (n < 0) 
         error("ERROR reading from socket");
    printf("%s\n",buffer);

    while(1) {
        printf("\n\nWhat would you like to do?\n");
        printf("[r]eceive or [s]end: ");

        char selection[3];
        fgets(selection, 2, stdin);
        strtok(selection, "\n");

        switch(selection[0]) {
            case 'r':
                while(1) {
                    bzero(buffer, 256);
                    buffer[0] = 49 + (current_sequence_number++ % 10);
                    buffer[1] = '1';  // GET action
                    n = write(sockfd,buffer, 256);
                    n = read(sockfd,buffer,255);

                    if (buffer[1] == ACK_RESPONSE) {
                        printf("No more messages to retreive!");
                        break;
                    }

                    printf("\nMessage From: %s\n",buffer+2);
                    printf("Message Received: %s\n",buffer+18);

                    bzero(buffer+2, 254);
                    buffer[1] = '3';
                    n = write(sockfd,buffer, 256);
                }
                break;

            case 's':
                bzero(buffer,256);
                buffer[0] = 49 + (current_sequence_number++ % 10);
                buffer[1] = '2';

                // Get destination IP address
                printf("Please enter the recipient's IP: ");
                fgets(buffer+2,16,stdin);
                // Bit crazy here: Remove any newline characters (NO NEW LINES PLZ)
                strtok(buffer+2, "\n");

                // Get message
                printf("Please enter the message: ");
                fgets(buffer+18,248,stdin);
                strtok(buffer+18, "\n");

                n = write(sockfd,buffer, 256);
                if (n < 0) 
                     error("ERROR writing to socket");
                bzero(buffer,256);
                n = read(sockfd,buffer,255);
                if (n < 0) 
                     error("ERROR reading from socket");

                if (((int)(buffer[0]) - 48) == current_sequence_number && buffer[1] == ACK_RESPONSE) {
                    printf("ACK successfully recieved.");
                } else {
                    printf("Server error occured...");
                }
                break;
            default:
                printf("invalid option!");
                break;
        }
    }


    return 0;
}

void listen_for_ACKs() {

}
