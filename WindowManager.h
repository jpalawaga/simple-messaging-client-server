#ifndef __WINDOWMANAGER_H_
#define __WINDOWMANAGER_H_

#include <string>
#include <deque>
#include <curses.h>

class WindowManager {

  private:
    std::deque<std::string> screenbuffer;
    WINDOW * window;

  public:
    void drawScreenBuffer() ;

    WindowManager() ;

    void write(std::string message);

    void end();

    void requestInput(std::string prompt, char* input, int length);

    void clearline(int line);

    void clearUserSpace();

    void refresh() ;
};

#endif