#include <stdint.h>
#include <math.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265358979323846264338327950f

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

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

struct game_sound_buffer {
    int16* Samples;
    int32 SamplesPerSecond;
    int32 SampleCount;
};

struct game_button_state {
    int32 HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input {
    bool32 IsAnalog;

    real32 StartX;
    real32 StartY;

    real32 MinX;
    real32 MinY;

    real32 MaxX;
    real32 MaxY;

    real32 EndX;
    real32 EndY;

    game_button_state Up;
    game_button_state Down;
    game_button_state Left;
    game_button_state Right;
    game_button_state LeftShoulder;
    game_button_state RightShoulder;
};

struct game_input {
    game_controller_input Controllers[4];
};

internal void GameOutputSound(game_sound_buffer* SoundBuffer, int32 ToneHz) {
    local_persist real32 tSine = 0;
    int32 ToneVolume = 2000;
    int32 WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
    int16* Samples = SoundBuffer->Samples;
    for (int32 SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex) {
        real32 SineValue = sinf(tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
        *Samples++ = SampleValue;
        *Samples++ = SampleValue;
        tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;
    }
}

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

internal void GameUpdateAndRender(
    game_input* Input,
    game_offscreen_buffer* Buffer,
    game_sound_buffer* SoundBuffer
) {
    local_persist int32 BlueOffset = 0;
    local_persist int32 GreenOffset = 0;
    local_persist int32 ToneHz = 256;

    game_controller_input* Input0 = &(Input->Controllers[0]);
    if (Input0->IsAnalog) {
        ToneHz = 256 + (int32)(Input0->EndY * 128.0f);
        BlueOffset += (int32)(Input0->EndX
            + 0 * 4.0f);
    } else {}

    if (Input0->Down.EndedDown) {
        GreenOffset++;
    }

    GameOutputSound(SoundBuffer, ToneHz);
    RenderWeirdGradient(Buffer, BlueOffset, GreenOffset);
}
