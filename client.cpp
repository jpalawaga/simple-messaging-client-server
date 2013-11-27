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

static const int FIELD_SEQ = 0;
static const int FIELD_TYPE = 1;
static const char RESPONSE_GET     = '1';
static const char RESPONSE_SEND    = '2';
static const char RESPONSE_ACK     = '3';
static const char RESPONSE_RECEIPT = '4';
static int current_sequence_number = 1;
static const int BUFF_SZ = 256;
static bool output_unlocked = true;

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
            std::this_thread::sleep_for (std::chrono::microseconds(100));
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
    char buffer[BUFF_SZ];

    while (1) {
        // We only want to process read receipts here.
        int readSocket = recvfrom(sockfd, &buffer, 255, MSG_DONTWAIT | MSG_PEEK, (struct sockaddr *) &cli_addr, &length_ptr);
        if (readSocket > 0 && buffer[FIELD_TYPE] == RESPONSE_RECEIPT) {
            int readSocket = recvfrom(sockfd, &buffer, 255, MSG_DONTWAIT, (struct sockaddr *) &cli_addr, &length_ptr);
            winMan->write("Message ID #"+string(buffer+2)+" has been delivered!");
            memset(buffer, 0, BUFF_SZ);
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
    char buffer[BUFF_SZ];
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    thread messageReciever(checkForReadReceipts, &winMan, sockfd);
    messageReciever.detach();

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
        buffer[FIELD_SEQ] = 49 + (current_sequence_number++ % 10);

        switch(selection[0]) {
            case 'r':
            case 'R':
                while(1) {
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
                buffer[FIELD_TYPE] = RESPONSE_SEND;

                // Get destination IP address
                winMan.requestInput("Please enter the recipient's IP: ", buffer+2, 16);

                // Get message
                winMan.requestInput("msg $> ", buffer+18, 247);

                n = write(sockfd,buffer, 256);
                if (n < 0) 
                     error("ERROR writing to socket");

                memset(buffer, 0, BUFF_SZ);
                n = read(sockfd, buffer, BUFF_SZ);
                if (n < 0) 
                     error("ERROR reading from socket");

                if (((int)(buffer[FIELD_SEQ]) - 48) == current_sequence_number && buffer[FIELD_TYPE] == RESPONSE_ACK) {
                    winMan.write("ACK successfully recieved. Message ID: "+string(buffer+2));
                } else {
                    winMan.write("Server error occured...");
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
