#include "handmade.h"
#include <math.h>

internal void GameOutputSound(game_sound_buffer* SoundBuffer) {
    int16* Samples = SoundBuffer->Samples;
    for (int32 SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex) {
        int16 SampleValue = 0;
        *Samples++ = SampleValue;
        *Samples++ = SampleValue;

    }
}

internal inline int32 RoundReal32ToInt32(real32 X) {
    return (int32)(X + 0.5f);
}

internal inline uint32 RoundReal32ToUint32(real32 X) {
    return (uint32)(X + 0.5f);
}

/// Maximums are not inclusive
internal void DrawRectangle(
    game_offscreen_buffer* Buffer,
    real32 RealMinX, real32 RealMinY, real32 RealMaxX, real32 RealMaxY,
    real32 R, real32 G, real32 B
) {
    int32 MinX = RoundReal32ToInt32(RealMinX);
    int32 MinY = RoundReal32ToInt32(RealMinY);
    int32 MaxX = RoundReal32ToInt32(RealMaxX);
    int32 MaxY = RoundReal32ToInt32(RealMaxY);

    if (MinX < 0) {
        MinX = 0;
    }
    if (MinY < 0) {
        MinY = 0;
    }

    if (MaxX > Buffer->Width) {
        MaxX = Buffer->Width;
    }
    if (MaxY > Buffer->Height) {
        MaxY = Buffer->Height;
    }

    uint32 Color = (RoundReal32ToUint32(R * 255.0f) << 16) |
        (RoundReal32ToUint32(G * 255.0f) << 8) |
        (RoundReal32ToUint32(B * 255.0f));

    uint8* Row = (uint8*)Buffer->Memory + MinY * Buffer->Pitch + MinX * Buffer->BytesPerPixel;
    for (int32 Y = MinY; Y < MaxY; ++Y) {
        uint32* Pixel = (uint32*)Row;
        for (int32 X = MinX; X < MaxX; ++X) {
            *Pixel++ = Color;
        }
        Row += Buffer->Pitch;
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(SoundBuffer);
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    Assert(
        &Input->Controllers[0].Terminator - &Input->Controllers[0].MoveUp ==
        ArrayCount(Input->Controllers[0].Buttons)
    );

    game_state* GameState = (game_state*)Memory->PermanentStorage;

    if (!Memory->IsInitialized) {

        Memory->IsInitialized = true;
    }

    for (int32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex) {
        game_controller_input* Controller = &(Input->Controllers[ControllerIndex]);
    }
    DrawRectangle(Buffer, 0, 0, (real32)Buffer->Width, (real32)Buffer->Height, 1.0f, 0.0f, 1.0f);
    DrawRectangle(Buffer, -10, 10, 70, 50, 0.0f, 1.0f, 1.0f);
}
