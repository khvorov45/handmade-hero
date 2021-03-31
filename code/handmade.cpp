#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265358979323846264338327950f

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef int32 bool32;

typedef float real32;
typedef double real64;

struct game_offscreen_buffer {
    void* Memory;
    int Width;
    int Height;
    int Pitch;
};

internal void RenderWeirdGradient(game_offscreen_buffer* Buffer, int BlueOffset, int GreenOffset) {
    int Width = Buffer->Width;
    uint8* Row = (uint8*)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; Y++) {
        uint32* Pixel = (uint32*)Row;

        for (int X = 0; X < Buffer->Width; X++) {
            uint8 Blue = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);
            *Pixel++ = (Green << 8 | Blue);
        }

        Row += Buffer->Pitch;
    }
}

internal void GameUpdateAndRender(game_offscreen_buffer* Buffer, int BlueOffset, int GreenOffset) {
    RenderWeirdGradient(Buffer, BlueOffset, GreenOffset);
}
