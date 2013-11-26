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
#include <thread>         // std::thread, std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds
#include <deque>

using namespace std;

static const char ACK_RESPONSE = '3';
static const char READ_RECIEPT = '4';
static bool screen_refresh_loop = true;
static bool read_refresh_loop = true;
static int current_sequence_number = 1;
static char buffer[256];
static bool output_unlocked = true;
static bool block_network = false;

void error(string msg)
{
    cout << msg;
    exit(0);
}

class WindowManager {

private:
    deque<string> screenbuffer;
    WINDOW * window;

public:
    static void drawScreenBuffer(WINDOW * test, deque<string> * sb) {
        while(1) {
            std::this_thread::sleep_for (std::chrono::microseconds(1000));
            if (output_unlocked) {
                wclear(test);
                for (int i = sb->size(); i > 0; i--) {
                    mvwprintw(test,i-1, 0, (char*)sb->at(i-1).c_str());
                }
                wrefresh(test);
            }
        }
    }
    WindowManager() {
        initscr();
        refresh();
        window = newwin(22, 94, 0, 0);
        thread bufferThread(drawScreenBuffer, window, &screenbuffer);
        bufferThread.detach();

    }

    void write(string message) {
        output_unlocked = false;
        if (screenbuffer.size() >= 22) {
            screenbuffer.pop_front();
        }
        screenbuffer.push_back(message);
        output_unlocked = true;
    }

    void end() {
        output_unlocked = false;
        endwin();
    }

    void requestInput(string prompt, char* input, int length) {

        clearUserSpace();
        mvprintw(23, 0, "%s", (char*)prompt.c_str());
        getnstr(input, length);
        strtok(input, "\n");
        clearUserSpace();
    }

    void clearline(int line) {
        output_unlocked = false;
        int y,x;
        getyx(window, y, x);
        move(line, 0);
        clrtoeol();
        move (y,x);
        wrefresh(window);
        output_unlocked = true;
    }

    void clearUserSpace() {
        clearline(23);
        clearline(24);
    }

    void refresh() {
        wrefresh(window);
    }
};

void checkForReadReceipts(WindowManager * winMan, int sockfd) {

    struct sockaddr_in serv_addr, cli_addr;
    unsigned length_ptr = 16;
    char buffer[256];

    while (1) {
        //std::this_thread::sleep_for (std::chrono::milliseconds(1));
        if (block_network == false) {
            int readSocket = recvfrom(sockfd, &buffer, 255, MSG_DONTWAIT, (struct sockaddr *) &cli_addr, &length_ptr);
            if (readSocket > 0 && buffer[1] == READ_RECIEPT) {
                winMan->write("Message ID #"+string(buffer+2)+" has been delivered!");
            }
        }
    }
}

int main(int argc, char *argv[])
{
    WindowManager winMan = WindowManager();

    int portno = 9009;
    int sockfd, n;

    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    thread messageReciever(checkForReadReceipts, &winMan, sockfd);
    messageReciever.detach();

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

    while(1) {
        std::this_thread::sleep_for (std::chrono::milliseconds(100));
        char selection[1];

        // Get destination IP address
        winMan.requestInput("[R]eceive | [S]end | e[X]it $> ", selection, 1);

        char buffer2[100];

        switch(selection[0]) {
            case 'r':
                while(1) {
                    bzero(buffer, 256);
                    buffer[0] = 49 + (current_sequence_number++ % 10);
                    buffer[1] = '1';  // GET action
                    n = write(sockfd,buffer, 256);
                    block_network = true;
                    n = read(sockfd,buffer,255);
                    block_network = false;

                    if (buffer[1] == ACK_RESPONSE) {
                        winMan.write("No more messages to retreive!");
                        break;
                    }
                    if (buffer[1] == READ_RECIEPT) {
                        winMan.write("Message from self received.");
                        continue;
                    }

                    winMan.write("");
                    sprintf(buffer2, "============ FROM : %s =============",buffer+2);
                    winMan.write(buffer2);
                    winMan.write(buffer+18);
                    winMan.write("==============================================");
                    winMan.write("");

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
                winMan.requestInput("Please enter the recipient's IP: ", buffer+2, 16);

                // Get message
                winMan.requestInput("msg $> ", buffer+18, 247);

                block_network = true;
                n = write(sockfd,buffer, 256);
                if (n < 0) 
                     error("ERROR writing to socket");
                bzero(buffer,256);
                n = read(sockfd,buffer,255);
                block_network = false;
                if (n < 0) 
                     error("ERROR reading from socket");

                if (((int)(buffer[0]) - 48) == current_sequence_number && buffer[1] == ACK_RESPONSE) {
                    winMan.write("ACK successfully recieved. Message ID: "+string(buffer+2));
                } else {
                    winMan.write("Server error occured...");
                }
                break;

                case 'x':
                    winMan.end();
                    return 0;
            default:
                winMan.write("invalid option!");
                break;
        }
    }


    return 0;
}

void listen_for_ACKs() {

}
