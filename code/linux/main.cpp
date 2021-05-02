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

    thread_context thread = {};

    game_memory game_memory = {};
    game_memory.PermanentStorageSize = Megabytes(64);
    game_memory.PermanentStorage = malloc(game_memory.PermanentStorageSize);

    game_input game_input = {};

    for (;;) {
        XEvent event;
        XNextEvent(window.display, &event); // XCheckWindowEvent doesn't block

        game_code.game_update_and_render(
            &thread, &game_memory, &game_input, &graphics_buffer.game_buffer
        );

        display_x11_graphics_buffer(&graphics_buffer, &window);
    }
    return 0;
}
