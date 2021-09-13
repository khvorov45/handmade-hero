#if !defined(HANDMADE_AUDIO_CPP)
#define HANDMADE_AUDIO_CPP

#include "asset.cpp"
#include "memory.cpp"

struct playing_sound {
    real32 Volume[2];
    sound_id ID;
    int32 SamplesPlayed;
    playing_sound* Next;
};

struct audio_state {
    memory_arena* PermArena;
    playing_sound* FirstPlayingSound;
    playing_sound* FirstFreePlayingSound;
};

internal void InitializeAudioState(audio_state* AudioState, memory_arena* Arena) {
    AudioState->FirstFreePlayingSound = 0;
    AudioState->FirstPlayingSound = 0;
    AudioState->PermArena = Arena;
}

internal void
OutputTestSineWave(game_sound_buffer* SoundBuffer, int32 ToneHz) {
    real32 ToneVolumeSine = 3000.0f;
    int32 WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
    int16* Samples = SoundBuffer->Samples;
    local_persist real32 tSine = 0.0f;
    local_persist uint32 TestSampleIndex = 0;
    for (int32 SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex) {
        real32 SineValue = sinf(tSine);
        int16 SampleValueSine = (int16)(SineValue * ToneVolumeSine);
        *Samples++ = SampleValueSine;
        *Samples++ = SampleValueSine;
        tSine += 2.0f * Pi32 * 1.0f / WavePeriod;
        if (tSine > 2.0f * Pi32) {
            tSine -= 2.0f * Pi32;
        }
    }
    TestSampleIndex += SoundBuffer->SampleCount;
}

internal playing_sound* PlaySound(audio_state* AudioState, sound_id SoundID) {
    if (!AudioState->FirstFreePlayingSound) {
        AudioState->FirstFreePlayingSound = PushStruct(AudioState->PermArena, playing_sound);
        AudioState->FirstFreePlayingSound->Next = 0;
    }

    playing_sound* PlayingSound = AudioState->FirstFreePlayingSound;
    AudioState->FirstFreePlayingSound = PlayingSound->Next;
    PlayingSound->Next = AudioState->FirstPlayingSound;
    AudioState->FirstPlayingSound = PlayingSound;

    PlayingSound->Volume[0] = 1.0f;
    PlayingSound->Volume[1] = 1.0f;
    PlayingSound->ID = SoundID;
    PlayingSound->SamplesPlayed = 0;
    return PlayingSound;
}

internal void PrefetchSound(game_assets* Assets, sound_id ID);
internal void LoadSound(game_assets* Assets, sound_id ID);
internal void OutputPlayingSounds(
    audio_state* AudioState, game_sound_buffer* SoundBuffer,
    game_assets* Assets, memory_arena* TempArena
) {
    temporary_memory MixelMemory = BeginTemporaryMemory(TempArena);

    real32* RealChannel0 = PushArray(MixelMemory.Arena, SoundBuffer->SampleCount, real32);
    real32* RealChannel1 = PushArray(MixelMemory.Arena, SoundBuffer->SampleCount, real32);

    // Clear mixer channels
    {
        real32* Dest0 = RealChannel0;
        real32* Dest1 = RealChannel1;
        for (int32 SampleIndex = 0;
            SampleIndex < SoundBuffer->SampleCount;
            ++SampleIndex) {

            *Dest0++ = 0.0f;
            *Dest1++ = 0.0f;
        }
    }

    // Sum all sounds
    for (playing_sound** PlayingSoundPtr = &AudioState->FirstPlayingSound;
        *PlayingSoundPtr;) {

        playing_sound* PlayingSound = *PlayingSoundPtr;
        bool32 SoundIsFinished = false;

        real32* Dest0 = RealChannel0;
        real32* Dest1 = RealChannel1;

        uint32 TotalSamplesToMix = SoundBuffer->SampleCount;
        while (!SoundIsFinished && TotalSamplesToMix) {
            loaded_sound* LoadedSound = GetSound(Assets, PlayingSound->ID);
            if (LoadedSound) {

                asset_sound_info* Info = GetSoundInfo(Assets, PlayingSound->ID);
                PrefetchSound(Assets, Info->NextIDToPlay);

                real32 Volume0 = PlayingSound->Volume[0];
                real32 Volume1 = PlayingSound->Volume[1];

                Assert(PlayingSound->SamplesPlayed >= 0);
                Assert(LoadedSound->SampleCount >= (uint32)PlayingSound->SamplesPlayed);

                uint32 SamplesToMix = TotalSamplesToMix;
                uint32 SamplesRemainingInSound = LoadedSound->SampleCount - PlayingSound->SamplesPlayed;
                if (SamplesToMix > SamplesRemainingInSound) {
                    SamplesToMix = SamplesRemainingInSound;
                }

                for (uint32 SampleIndex = PlayingSound->SamplesPlayed;
                    SampleIndex < PlayingSound->SamplesPlayed + SamplesToMix;
                    ++SampleIndex) {

                    real32 SampleValue0 = LoadedSound->Samples[0][SampleIndex];
                    *Dest0++ += SampleValue0 * Volume0;
                    *Dest1++ += SampleValue0 * Volume1;
                }

                Assert(TotalSamplesToMix >= SamplesToMix);
                PlayingSound->SamplesPlayed += SamplesToMix;
                TotalSamplesToMix -= SamplesToMix;

                if ((uint32)PlayingSound->SamplesPlayed == LoadedSound->SampleCount) {
                    if (IsValid(Info->NextIDToPlay)) {
                        PlayingSound->ID = Info->NextIDToPlay;
                        PlayingSound->SamplesPlayed = 0;
                    } else {
                        SoundIsFinished = true;
                    }
                } else {
                    Assert(TotalSamplesToMix == 0);
                }
            } else {
                LoadSound(Assets, PlayingSound->ID);
                break;
            }
        }

        if (SoundIsFinished) {
            *PlayingSoundPtr = PlayingSound->Next;
            PlayingSound->Next = AudioState->FirstFreePlayingSound;
            AudioState->FirstFreePlayingSound = PlayingSound;
        } else {
            PlayingSoundPtr = &PlayingSound->Next;
        }
    }

    // Convert to 16 bit
    {
        real32* Source0 = RealChannel0;
        real32* Source1 = RealChannel1;
        int16* SampleOut = SoundBuffer->Samples;
        for (int32 SampleIndex = 0;
            SampleIndex < SoundBuffer->SampleCount;
            ++SampleIndex) {

            *SampleOut++ = (int16)(*Source0++ + 0.5f);
            *SampleOut++ = (int16)(*Source1++ + 0.5f);
        }
    }

    EndTemporaryMemory(MixelMemory);
}

#endif
