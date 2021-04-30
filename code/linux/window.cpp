#include "../types.h"
#include <X11/Xlib.h>

struct X11Window {
    Display* display;
    Window window;
    GC gc;
};

X11Window create_window(uint32 width, uint32 height) {
    X11Window window;
    window.display = XOpenDisplay(0);
    Window root_window = DefaultRootWindow(window.display);
    int32 screen_default = DefaultScreen(window.display);
    int32 blackColor = BlackPixel(window.display, screen_default);
    window.window = XCreateSimpleWindow(
        window.display, root_window, 0, 0, width, height, 0,
        blackColor, blackColor
    );
    XSelectInput(window.display, window.window, ButtonPressMask | KeyPressMask);
    XMapWindow(window.display, window.window);
    window.gc = XCreateGC(window.display, window.window, 0, 0);
    XSetForeground(window.display, window.gc, blackColor);
    return window;
}
