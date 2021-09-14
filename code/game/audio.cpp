#if !defined(HANDMADE_AUDIO_CPP)
#define HANDMADE_AUDIO_CPP

#include "asset.cpp"
#include "memory.cpp"

struct playing_sound {
    v2 CurrentVolume;
    v2 dCurrentVolume;
    v2 TargetVolume;
    sound_id ID;
    int32 SamplesPlayed;
    playing_sound* Next;
};

struct audio_state {
    memory_arena* PermArena;
    playing_sound* FirstPlayingSound;
    playing_sound* FirstFreePlayingSound;
    v2 MasterVolume;
};

internal void InitializeAudioState(audio_state* AudioState, memory_arena* Arena) {
    AudioState->FirstFreePlayingSound = 0;
    AudioState->FirstPlayingSound = 0;
    AudioState->PermArena = Arena;
    AudioState->MasterVolume = V2(1.0f, 1.0f);
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

    PlayingSound->CurrentVolume = PlayingSound->TargetVolume = V2(1.0f, 1.0f);
    PlayingSound->dCurrentVolume = V2(0, 0);
    PlayingSound->ID = SoundID;
    PlayingSound->SamplesPlayed = 0;
    return PlayingSound;
}

internal void ChangeVolume(
    audio_state* AudioState, playing_sound* Sound, real32 FadeDurationInSeconds, v2 Volume
) {
    if (FadeDurationInSeconds <= 0.0f) {
        Sound->CurrentVolume = Sound->TargetVolume = Volume;
    } else {
        Sound->TargetVolume = Volume;
        Sound->dCurrentVolume = (Volume - Sound->CurrentVolume) * (1.0f / FadeDurationInSeconds);
    }
}

internal void PrefetchSound(game_assets* Assets, sound_id ID);
internal void LoadSound(game_assets* Assets, sound_id ID);
internal void OutputPlayingSounds(
    audio_state* AudioState, game_sound_buffer* SoundBuffer,
    game_assets* Assets, memory_arena* TempArena
) {
    temporary_memory MixerMemory = BeginTemporaryMemory(TempArena);

    real32* RealChannel0 = PushArray(MixerMemory.Arena, SoundBuffer->SampleCount, real32);
    real32* RealChannel1 = PushArray(MixerMemory.Arena, SoundBuffer->SampleCount, real32);

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

        real32 SecondsPerSample = 1.0f / (real32)SoundBuffer->SamplesPerSecond;

        uint32 TotalSamplesToMix = SoundBuffer->SampleCount;
        while (!SoundIsFinished && TotalSamplesToMix) {
            loaded_sound* LoadedSound = GetSound(Assets, PlayingSound->ID);
            if (LoadedSound) {

                asset_sound_info* Info = GetSoundInfo(Assets, PlayingSound->ID);
                PrefetchSound(Assets, Info->NextIDToPlay);

                v2 Volume = PlayingSound->CurrentVolume;
                v2 dVolume = PlayingSound->dCurrentVolume * SecondsPerSample;

                Assert(PlayingSound->SamplesPlayed >= 0);
                Assert(LoadedSound->SampleCount >= (uint32)PlayingSound->SamplesPlayed);

                uint32 SamplesToMix = TotalSamplesToMix;
                uint32 SamplesRemainingInSound = LoadedSound->SampleCount - PlayingSound->SamplesPlayed;
                if (SamplesToMix > SamplesRemainingInSound) {
                    SamplesToMix = SamplesRemainingInSound;
                }
#define OutputChannelCount 2
                bool32 VolumeEnded[OutputChannelCount] = {};
                for (uint32 ChannelIndex = 0; ChannelIndex < OutputChannelCount; ++ChannelIndex) {
                    if (dVolume.E[ChannelIndex] != 0.0f) {
                        real32 DeltaVolume =
                            PlayingSound->TargetVolume.E[ChannelIndex] - Volume.E[ChannelIndex];
                        uint32 VolumeSampleCount = (uint32)(DeltaVolume / dVolume.E[ChannelIndex] + 0.5f);
                        if (SamplesToMix >= VolumeSampleCount) {
                            SamplesToMix = VolumeSampleCount;
                            VolumeEnded[ChannelIndex] = true;
                        }
                    }
                }

                for (uint32 SampleIndex = PlayingSound->SamplesPlayed;
                    SampleIndex < PlayingSound->SamplesPlayed + SamplesToMix;
                    ++SampleIndex) {

                    real32 SampleValue0 = LoadedSound->Samples[0][SampleIndex];
                    *Dest0++ += SampleValue0 * Volume.E[0] * AudioState->MasterVolume.E[0];
                    *Dest1++ += SampleValue0 * Volume.E[1] * AudioState->MasterVolume.E[1];

                    Volume += dVolume;
                }

                PlayingSound->CurrentVolume = Volume;

                for (uint32 ChannelIndex = 0; ChannelIndex < OutputChannelCount; ++ChannelIndex) {
                    if (VolumeEnded[ChannelIndex]) {
                        PlayingSound->CurrentVolume.E[ChannelIndex] =
                            PlayingSound->TargetVolume.E[ChannelIndex];
                        PlayingSound->dCurrentVolume.E[ChannelIndex] = 0.0f;
                    }
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

    EndTemporaryMemory(MixerMemory);
}

#endif
