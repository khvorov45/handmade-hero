#include "../types.h"
#include "window.cpp"
#include "graphics.cpp"
#include "gamecode.cpp"
#include <X11/Xlib.h>

int main() {
    uint32 width = 960;
    uint32 height = 540;
    X11Window window = create_window(width - 100, height);
    X11GraphicsBuffer graphics_buffer = create_x11_graphics_buffer(&window, width, height);

    GameCode game_code = load_game_code();

    for (;;) {
        XEvent event;
        XNextEvent(window.display, &event); // XCheckWindowEvent doesn't block

        draw(graphics_buffer.game_buffer.Memory, width, height);
        display_x11_graphics_buffer(&graphics_buffer, &window);
    }
    return 0;
}
