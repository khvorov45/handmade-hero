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

internal void RenderRectangle(game_offscreen_buffer* Buffer, int32 XCoord, int32 YCoord) {
    uint8* EndOfBuffer = (uint8*)Buffer->Memory + Buffer->Pitch * Buffer->Height;
    int32 Top = YCoord;
    int32 Bottom = YCoord + 10;
    uint32 Color = 0x000000;
    for (int32 X = XCoord; X < XCoord + 10; ++X) {
        uint8* Pixel =
            (uint8*)Buffer->Memory + X * Buffer->BytesPerPixel
            + Top * Buffer->Pitch;
        for (int Y = Top; Y < Bottom; ++Y) {
            if (Pixel >= Buffer->Memory && Pixel < EndOfBuffer) {
                *(uint32*)Pixel = Color;
                Pixel += Buffer->Pitch;
            }
        }
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
}
