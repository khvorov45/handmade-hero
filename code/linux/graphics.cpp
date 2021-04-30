#include "../types.h"
#include "window.cpp"
#include "stdlib.h"
#include <X11/Xlib.h>

struct X11GraphicsBuffer {
    XImage* ximage;
    uint8* memory;
};

void draw(uint8* rgb_out, int32 width, int32 height) {
    uint32* pixel = (uint32*)rgb_out;
    for (int32 i = 0; i < width * height; i += 1) {
        *pixel++ = 0x000FF;
    }
    return;
}

X11GraphicsBuffer create_x11_graphics_buffer(X11Window* window, int32 width, int32 height) {
    X11GraphicsBuffer buffer = {};
    buffer.memory = (uint8*)malloc(width * height * 4);
    buffer.ximage = XCreateImage(
        window->display, window->visual, 24,
        ZPixmap, 0, (char*)buffer.memory,
        width, height, 32, 0
    );
    return buffer;
}

void display_x11_graphics_buffer(
    X11GraphicsBuffer* graphics_buffer, X11Window* window, int32 width, int32 height
) {
    XPutImage(
        window->display, window->window,
        window->gc, graphics_buffer->ximage, 0, 0, 0, 0,
        width, height
    );
}
