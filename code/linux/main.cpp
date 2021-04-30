#include "../types.h"
#include "window.cpp"
#include "graphics.cpp"
#include "stdlib.h"
#include <X11/Xlib.h>

int main() {
    uint32 width = 960;
    uint32 height = 540;
    X11Window window = create_window(width, height);
    X11GraphicsBuffer graphics_buffer = create_x11_graphics_buffer(&window, width, height);

    for (;;) {
        XEvent event;
        XNextEvent(window.display, &event); // XCheckWindowEvent doesn't block

        draw(graphics_buffer.memory, width, height);
        display_x11_graphics_buffer(&graphics_buffer, &window, width, height);
    }
    return 0;
}
