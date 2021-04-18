#include "handmade.h"
#include <math.h>

internal void GameOutputSound(game_sound_buffer* SoundBuffer, game_state* GameState) {
    int32 ToneVolume = 2000;
    int32 WavePeriod = SoundBuffer->SamplesPerSecond / GameState->ToneHz;
    int16* Samples = SoundBuffer->Samples;
    real32 Pi2 = 2.0f * Pi32;
    for (int32 SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex) {
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
        *Samples++ = SampleValue;
        *Samples++ = SampleValue;
        GameState->tSine += Pi2 * 1.0f / (real32)WavePeriod;
        if (GameState->tSine >= Pi2) {
            GameState->tSine -= Pi2;
        }
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

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(SoundBuffer, GameState);
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    Assert(
        &Input->Controllers[0].Terminator - &Input->Controllers[0].MoveUp ==
        ArrayCount(Input->Controllers[0].Buttons)
    );

    game_state* GameState = (game_state*)Memory->PermanentStorage;

    if (!Memory->IsInitialized) {

        char* Filename = __FILE__;

        debug_read_file_result BitmapMemory = Memory->DEBUGPlatformReadEntireFile(Filename);
        if (BitmapMemory.Contents) {
            Memory->DEBUGPlatformWriteEntireFile("test_write.out", BitmapMemory.Size, BitmapMemory.Contents);
            Memory->DEBUGPlatformFreeFileMemory(BitmapMemory.Contents);
        }

        GameState->ToneHz = 256;
        GameState->tSine = 0;

        GameState->BlueOffset = 0;
        GameState->GreenOffset = 0;
        Memory->IsInitialized = true;
    }

    for (int32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex) {
        game_controller_input* Controller = &(Input->Controllers[ControllerIndex]);
        if (Controller->IsAnalog) {
            GameState->ToneHz = 256 + (int32)(Controller->StickAverageY * 128.0f);
            GameState->BlueOffset += (int32)(Controller->StickAverageX * 4.0f);
        } else {
            if (Controller->MoveLeft.EndedDown) {
                GameState->BlueOffset--;
            }
            if (Controller->MoveRight.EndedDown) {
                GameState->BlueOffset++;
            }
        }

        if (Controller->ActionDown.EndedDown) {
            GameState->GreenOffset++;
        }
    }
    RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}
