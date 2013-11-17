/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    int portno = 9009;
    int sockfd, newsockfd, clilen;
    char buffer[256];
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
        printf("From: %u\n", cli_addr.sin_addr.s_addr);
        printf("Incoming Data : %s\n",buffer);

        sendto (sockfd, "Server has received your datagram.", 255, 0, (struct sockaddr *) &cli_addr, length_ptr);
     }
     
     return 0; 
}