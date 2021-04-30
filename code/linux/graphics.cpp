#include "../types.h"
#include "window.cpp"
#include "../game/lib.hpp"
#include "stdlib.h"
#include <X11/Xlib.h>

struct X11GraphicsBuffer {
    XImage* ximage;
    game_offscreen_buffer game_buffer;
};

void draw(void* rgb_out, int32 width, int32 height) {
    uint32* pixel = (uint32*)rgb_out;
    for (int32 i = 0; i < width * height; i += 1) {
        *pixel++ = 0x000FF;
    }
    return;
}

X11GraphicsBuffer create_x11_graphics_buffer(X11Window* window, int32 width, int32 height) {
    X11GraphicsBuffer buffer = {};

    buffer.game_buffer.BytesPerPixel = 4;
    buffer.game_buffer.Height = height;
    buffer.game_buffer.Width = width;
    buffer.game_buffer.Pitch = width * buffer.game_buffer.BytesPerPixel;
    buffer.game_buffer.Memory =
        (uint8*)malloc(width * height * buffer.game_buffer.BytesPerPixel);

    buffer.ximage = XCreateImage(
        window->display, window->visual, 24,
        ZPixmap, 0, (char*)buffer.game_buffer.Memory,
        width, height, 32, 0
    );
    return buffer;
}

void display_x11_graphics_buffer(
    X11GraphicsBuffer* graphics_buffer, X11Window* window
) {
    XPutImage(
        window->display, window->window,
        window->gc, graphics_buffer->ximage, 0, 0, 0, 0,
        graphics_buffer->game_buffer.Width, graphics_buffer->game_buffer.Height
    );
}
