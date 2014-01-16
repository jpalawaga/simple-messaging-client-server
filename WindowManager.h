#ifndef __WINDOWMANAGER_H_
#define __WINDOWMANAGER_H_

#include <string>
#include <deque>
#include <curses.h>

class WindowManager {

  private:
    std::deque<std::string> screenbuffer;
    WINDOW * window;

    void drawScreenBuffer();

    void clearline(int line);

    void clearUserSpace();

  public:

    WindowManager();

    void write(std::string message);

    void requestInput(std::string prompt, char* input, int length);

    void end();

};

#endif