/**
 * HANDMADE_INTERNAL:
 *  0 - Public release
 *  1 - Developer
 *
 * HANDMADE_SLOW:
 *  0 - No slow code allowed
 *  1 - Slow code allowed
*/

#include <stdint.h>
#include <math.h>

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

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265358979323846264338327950f

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Kilobytes(n) (((uint64)(n)) * 1024)
#define Megabytes(n) (Kilobytes(n) * 1024)
#define Gigabytes(n) (Megabytes(n) * 1024)
#define Terabytes(n) (Gigabytes(n) * 1024)

#if HANDMADE_SLOW
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif


#if HANDMADE_INTERNAL
struct debug_read_file_result {
    void* Contents;
    uint32 Size;
};

internal debug_read_file_result DEBUGPlatformReadEntireFile(char* Filename);
internal void DEBUGPlatformFreeFileMemory(void* BitmapMemory);
internal bool32 DEBUGPlatformWriteEntireFile(char* Filename, uint32 MemorySize, void* Memory);
#else
#endif

struct game_state {
    int32 BlueOffset;
    int32 GreenOffset;
    int32 ToneHz;
};

struct game_memory {
    bool32 IsInitialized;
    uint64 PermanentStorageSize;
    void* PermanentStorage; //* Required to be cleared to 0
    uint64 TransientStorageSize;
    void* TransientStorage; //* Required to be cleared to 0
};

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

inline uint32 SafeTruncateUint64(uint64 Value) {
    Assert(Value <= 0xFFFFFFFF);
    return (uint32)(Value);
}

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
    uint8* Row = (uint8*)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; Y++) {
        uint32* Pixel = (uint32*)Row;

        for (int X = 0; X < Buffer->Width; X++) {
            uint8 Blue = (uint8)(X + BlueOffset);
            uint8 Green = (uint8)(Y + GreenOffset);
            *Pixel++ = (Green << 8 | Blue);
        }

        Row += Buffer->Pitch;
    }
}

internal void GameUpdateAndRender(
    game_memory* Memory,
    game_input* Input,
    game_offscreen_buffer* Buffer,
    game_sound_buffer* SoundBuffer
) {
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state* GameState = (game_state*)Memory->PermanentStorage;

    if (!Memory->IsInitialized) {

        char* Filename = __FILE__;

        debug_read_file_result BitmapMemory = DEBUGPlatformReadEntireFile(Filename);
        if (BitmapMemory.Contents) {
            DEBUGPlatformWriteEntireFile("test_write.out", BitmapMemory.Size, BitmapMemory.Contents);
            DEBUGPlatformFreeFileMemory(BitmapMemory.Contents);
        }

        GameState->ToneHz = 256;
        GameState->BlueOffset = 0;
        GameState->GreenOffset = 0;
        Memory->IsInitialized = true;
    }

    game_controller_input* Input0 = &(Input->Controllers[0]);
    if (Input0->IsAnalog) {
        GameState->ToneHz = 256 + (int32)(Input0->EndY * 128.0f);
        GameState->BlueOffset += (int32)(Input0->EndX * 4.0f);
    } else {}

    if (Input0->Down.EndedDown) {
        GameState->GreenOffset++;
    }

    GameOutputSound(SoundBuffer, GameState->ToneHz);
    RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}
