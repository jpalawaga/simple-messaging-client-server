#include "WindowManager.h"

void WindowManager::end() {
    endwin();
}

void WindowManager::drawScreenBuffer() {
    wclear(window);
    for (int i = screenbuffer.size(); i > 0; i--) {
        mvwprintw(window, i-1, 0, (char*)screenbuffer.at(i-1).c_str());
    }
    wrefresh(window);
}

WindowManager::WindowManager() {
    initscr();
    wrefresh(window);
    window = newwin(22, 94, 0, 0);
}

void WindowManager::write(std::string message) {
    if (screenbuffer.size() >= 22) {
        screenbuffer.pop_front();
    }
    screenbuffer.push_back(message);
    drawScreenBuffer();
}

void WindowManager::requestInput(std::string prompt, char* input, int length) {

    clearUserSpace();
    mvprintw(23, 0, "%s", (char*)prompt.c_str());
    getnstr(input, length);
    strtok(input, "\n");
    clearUserSpace();
}

void WindowManager::clearline(int line) {
    int y,x;
    getyx(window, y, x);
    move(line, 0);
    clrtoeol();
    move (y,x);
    wrefresh(window);
}

void WindowManager::clearUserSpace() {
    clearline(23);
    clearline(24);
}
