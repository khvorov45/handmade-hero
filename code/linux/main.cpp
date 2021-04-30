#include "../types.h"
#include "window.cpp"
#include <X11/Xlib.h>

int main() {
    uint32 width = 960;
    uint32 height = 540;
    X11Window window = create_window(width, height);

    for (;;) {
        XEvent e;
        XNextEvent(window.display, &e);
        if (e.type == MapNotify) break;
    }
    return 0;
}
