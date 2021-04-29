#include <X11/Xlib.h>

int main() {
    Display* display = XOpenDisplay(0);
    if (display == NULL) {
        return (1);
    }
    Window root = DefaultRootWindow(display);
    int screen = DefaultScreen(display);
    int blackColor = BlackPixel(display, screen);
    Window window = XCreateSimpleWindow(
        display, root, 0, 0, 200, 200, 0,
        blackColor, blackColor
    );
    XSelectInput(display, window, StructureNotifyMask);
    XMapWindow(display, window);
    GC gc = XCreateGC(display, window, 0, 0);
    XSetForeground(display, gc, blackColor);
    for (;;) {
        XEvent e;
        XNextEvent(display, &e);
        if (e.type == MapNotify) break;
    }
    return 0;
}
