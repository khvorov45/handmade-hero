#if !defined(HANDMADE_AUDIO_CPP)
#define HANDMADE_AUDIO_CPP

#include "asset.cpp"
#include "memory.cpp"
#include "../intrinsics.h"

struct playing_sound {
    v2 CurrentVolume;
    v2 dCurrentVolume;
    v2 TargetVolume;
    real32 dSample;
    sound_id ID;
    real32 SamplesPlayed;
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
    AudioState->MasterVolume = V2(0.5f, 0.5f);
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
    PlayingSound->dSample = 1.0f;
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

internal void ChangePitch(audio_state* AudioState, playing_sound* Sound, real32 dSample) {
    Sound->dSample = dSample;
}

internal void PrefetchSound(game_assets* Assets, sound_id ID);
internal void LoadSound(game_assets* Assets, sound_id ID);
internal void OutputPlayingSounds(
    audio_state* AudioState, game_sound_buffer* SoundBuffer,
    game_assets* Assets, memory_arena* TempArena
) {
    temporary_memory MixerMemory = BeginTemporaryMemory(TempArena);

    Assert((SoundBuffer->SampleCount & 7) == 0);
    uint32 SampleCount8 = SoundBuffer->SampleCount / 8;
    uint32 SampleCount4 = SoundBuffer->SampleCount / 4;

    __m128* RealChannel0 = PushArray(MixerMemory.Arena, SampleCount4, __m128, 16);
    __m128* RealChannel1 = PushArray(MixerMemory.Arena, SampleCount4, __m128, 16);

    __m128 Zero_4x = _mm_set1_ps(0.0f);

    // Clear mixer channels
    {
        __m128* Dest0 = RealChannel0;
        __m128* Dest1 = RealChannel1;
        for (uint32 SampleIndex = 0;
            SampleIndex < SampleCount4;
            ++SampleIndex) {

            _mm_store_ps((real32*)Dest0++, Zero_4x);
            _mm_store_ps((real32*)Dest1++, Zero_4x);
        }
    }

    // Sum all sounds
    for (playing_sound** PlayingSoundPtr = &AudioState->FirstPlayingSound;
        *PlayingSoundPtr;) {

        playing_sound* PlayingSound = *PlayingSoundPtr;
        bool32 SoundIsFinished = false;

        __m128* Dest0 = RealChannel0;
        __m128* Dest1 = RealChannel1;
        real32* RDest0 = (real32*)Dest0;
        real32* RDest1 = (real32*)Dest1;

        real32 SecondsPerSample = 1.0f / (real32)SoundBuffer->SamplesPerSecond;

        uint32 TotalSamplesToMix8 = SampleCount8;
        while (!SoundIsFinished && TotalSamplesToMix8) {
            loaded_sound* LoadedSound = GetSound(Assets, PlayingSound->ID);
            if (LoadedSound) {

                asset_sound_info* Info = GetSoundInfo(Assets, PlayingSound->ID);
                PrefetchSound(Assets, Info->NextIDToPlay);

                v2 Volume = PlayingSound->CurrentVolume;
                v2 dVolume = PlayingSound->dCurrentVolume * SecondsPerSample;
                v2 dVolume8 = 8.0f * dVolume;
                real32 dSample8 = 8.0f * PlayingSound->dSample;
                real32 dSample = PlayingSound->dSample;

                __m128 MasterVolume0_4x = _mm_set1_ps(AudioState->MasterVolume.E[0]);
                __m128 MasterVolume1_4x = _mm_set1_ps(AudioState->MasterVolume.E[1]);
                __m128 Volume0_4x = _mm_setr_ps(
                    Volume.E[0] + 0.0f * dVolume.E[0],
                    Volume.E[0] + 1.0f * dVolume.E[0],
                    Volume.E[0] + 2.0f * dVolume.E[0],
                    Volume.E[0] + 3.0f * dVolume.E[0]
                );
                __m128 Volume1_4x = _mm_setr_ps(
                    Volume.E[1] + 0.0f * dVolume.E[1],
                    Volume.E[1] + 1.0f * dVolume.E[1],
                    Volume.E[1] + 2.0f * dVolume.E[1],
                    Volume.E[1] + 3.0f * dVolume.E[1]
                );
                __m128 dVolume0_4x = _mm_set1_ps(dVolume.E[0]);
                __m128 dVolume1_4x = _mm_set1_ps(dVolume.E[1]);
                __m128 dVolume80_4x = _mm_set1_ps(dVolume8.E[0]);
                __m128 dVolume81_4x = _mm_set1_ps(dVolume8.E[1]);

                Assert(PlayingSound->SamplesPlayed >= 0);

                uint32 SamplesToMix8 = TotalSamplesToMix8;
                real32 RealSampleRemainingInSound8 =
                    (LoadedSound->SampleCount - RoundReal32ToInt32(PlayingSound->SamplesPlayed)) / dSample8;
                uint32 SamplesRemainingInSound8 = RoundReal32ToInt32(RealSampleRemainingInSound8);
                if (SamplesToMix8 > SamplesRemainingInSound8) {
                    SamplesToMix8 = SamplesRemainingInSound8;
                }
#define OutputChannelCount 2
                bool32 VolumeEnded[OutputChannelCount] = {};
                for (uint32 ChannelIndex = 0; ChannelIndex < OutputChannelCount; ++ChannelIndex) {
                    if (dVolume8.E[ChannelIndex] != 0.0f) {
                        real32 DeltaVolume =
                            PlayingSound->TargetVolume.E[ChannelIndex] - Volume.E[ChannelIndex];
                        uint32 VolumeSampleCount8 = (uint32)(0.125f * (DeltaVolume / dVolume8.E[ChannelIndex] + 0.5f));
                        if (SamplesToMix8 >= VolumeSampleCount8) {
                            SamplesToMix8 = VolumeSampleCount8;
                            VolumeEnded[ChannelIndex] = true;
                        }
                    }
                }

                real32 SamplePosition = PlayingSound->SamplesPlayed;
                for (uint32 LoopIndex = 0; LoopIndex < SamplesToMix8; ++LoopIndex) {
#if 0
                    for (uint32 SampleOffset = 0; SampleOffset < 8; ++SampleOffset) {
#if 0
                        real32 OffsetSamplePosition = SamplePosition + (real32)SampleOffset * dSample;
                        uint32 SampleIndex = FloorReal32ToInt32(OffsetSamplePosition);

                        real32 Between = OffsetSamplePosition - (real32)SampleIndex;

                        real32 SampleValueLow0 = LoadedSound->Samples[0][SampleIndex];
                        real32 SampleValueHigh0 = LoadedSound->Samples[0][SampleIndex + 1];

                        real32 SampleValue0 = Lerp(SampleValueLow0, Between, SampleValueHigh0);
#else
                        uint32 SampleIndex = RoundReal32ToInt32(SamplePosition + SampleOffset * dSample);
                        real32 SampleValue0 = LoadedSound->Samples[0][SampleIndex];
#endif
                        * RDest0++ += SampleValue0 * Volume.E[0] * AudioState->MasterVolume.E[0];
                        *RDest1++ += SampleValue0 * Volume.E[1] * AudioState->MasterVolume.E[1];
                        Dest0++;
                        Dest1++;
                    }
#else
                    __m128 SampleValue0_0 = _mm_setr_ps(
                        LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 0.0f * dSample)],
                        LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 1.0f * dSample)],
                        LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 2.0f * dSample)],
                        LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 3.0f * dSample)]
                    );
                    __m128 SampleValue0_1 = _mm_setr_ps(
                        LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 4.0f * dSample)],
                        LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 5.0f * dSample)],
                        LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 6.0f * dSample)],
                        LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 7.0f * dSample)]
                    );

                    __m128 D0_0 = _mm_load_ps((real32*)&Dest0[0]);
                    __m128 D0_1 = _mm_load_ps((real32*)&Dest0[1]);
                    __m128 D1_0 = _mm_load_ps((real32*)&Dest1[0]);
                    __m128 D1_1 = _mm_load_ps((real32*)&Dest1[1]);

                    D0_0 = _mm_add_ps(D0_0, _mm_mul_ps(_mm_mul_ps(MasterVolume0_4x, Volume0_4x), SampleValue0_0));
                    D0_1 = _mm_add_ps(D0_1, _mm_mul_ps(_mm_mul_ps(MasterVolume0_4x, _mm_add_ps(Volume0_4x, dVolume0_4x)), SampleValue0_1));
                    D1_0 = _mm_add_ps(D1_0, _mm_mul_ps(_mm_mul_ps(MasterVolume1_4x, Volume1_4x), SampleValue0_0));
                    D1_1 = _mm_add_ps(D1_1, _mm_mul_ps(_mm_mul_ps(MasterVolume1_4x, _mm_add_ps(Volume1_4x, dVolume1_4x)), SampleValue0_1));

                    _mm_store_ps((real32*)&Dest0[0], D0_0);
                    _mm_store_ps((real32*)&Dest0[1], D0_1);
                    _mm_store_ps((real32*)&Dest1[0], D1_0);
                    _mm_store_ps((real32*)&Dest1[1], D1_1);

                    Dest0 += 2;
                    Dest1 += 2;
                    Volume0_4x = _mm_add_ps(Volume0_4x, dVolume80_4x);
                    Volume1_4x = _mm_add_ps(Volume1_4x, dVolume81_4x);
#endif

                    Volume += dVolume8;
                    SamplePosition += dSample8;
                }

                PlayingSound->CurrentVolume = Volume;

                for (uint32 ChannelIndex = 0; ChannelIndex < OutputChannelCount; ++ChannelIndex) {
                    if (VolumeEnded[ChannelIndex]) {
                        PlayingSound->CurrentVolume.E[ChannelIndex] =
                            PlayingSound->TargetVolume.E[ChannelIndex];
                        PlayingSound->dCurrentVolume.E[ChannelIndex] = 0.0f;
                    }
                }

                PlayingSound->SamplesPlayed = SamplePosition;
                Assert(TotalSamplesToMix8 >= SamplesToMix8);
                TotalSamplesToMix8 -= SamplesToMix8;

                if ((uint32)PlayingSound->SamplesPlayed >= LoadedSound->SampleCount) {
                    if (IsValid(Info->NextIDToPlay)) {
                        PlayingSound->ID = Info->NextIDToPlay;
                        PlayingSound->SamplesPlayed -= (real32)LoadedSound->SampleCount;
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
        __m128* Source0 = RealChannel0;
        __m128* Source1 = RealChannel1;
        __m128i* SampleOut = (__m128i*)SoundBuffer->Samples;
        for (uint32 SampleIndex = 0;
            SampleIndex < SampleCount4;
            ++SampleIndex) {

            __m128 S0 = _mm_load_ps((real32*)Source0++);
            __m128 S1 = _mm_load_ps((real32*)Source1++);

            __m128i L = _mm_cvtps_epi32(S0);
            __m128i R = _mm_cvtps_epi32(S1);

            __m128i LR0 = _mm_unpacklo_epi32(L, R);
            __m128i LR1 = _mm_unpackhi_epi32(L, R);

            __m128i S01 = _mm_packs_epi32(LR0, LR1);

            _mm_store_si128(SampleOut++, S01);
            // *SampleOut++ = S01;
        }
    }

    EndTemporaryMemory(MixerMemory);
}

#endif
