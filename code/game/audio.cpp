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

    Assert((SoundBuffer->SampleCount & 3) == 0);
    uint32 ChunkCount = SoundBuffer->SampleCount / 4;

    __m128* RealChannel0 = PushArray(MixerMemory.Arena, ChunkCount, __m128, 16);
    __m128* RealChannel1 = PushArray(MixerMemory.Arena, ChunkCount, __m128, 16);

    // Clear mixer channels
    {
        __m128 Zero_4x = _mm_set1_ps(0.0f);
        __m128* Dest0 = RealChannel0;
        __m128* Dest1 = RealChannel1;
        for (uint32 SampleIndex = 0;
            SampleIndex < ChunkCount;
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

        uint32 TotalChunksToMix = ChunkCount;
        while (!SoundIsFinished && TotalChunksToMix) {
            loaded_sound* LoadedSound = GetSound(Assets, PlayingSound->ID);
            if (LoadedSound) {

                asset_sound_info* Info = GetSoundInfo(Assets, PlayingSound->ID);
                PrefetchSound(Assets, Info->NextIDToPlay);

                v2 Volume = PlayingSound->CurrentVolume;
                v2 dVolume = PlayingSound->dCurrentVolume * SecondsPerSample;
                v2 dVolumeChunk = 4.0f * dVolume;
                real32 dSample = PlayingSound->dSample;
                real32 dSampleChunk = 4.0f * PlayingSound->dSample;

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
                __m128 dVolumeChunk0_4x = _mm_set1_ps(dVolumeChunk.E[0]);
                __m128 dVolumeChunk1_4x = _mm_set1_ps(dVolumeChunk.E[1]);

                Assert(PlayingSound->SamplesPlayed >= 0);

                uint32 ChunksToMix = TotalChunksToMix;
                real32 RealChunksRemainingInSound =
                    (LoadedSound->SampleCount - RoundReal32ToInt32(PlayingSound->SamplesPlayed)) / dSampleChunk;
                uint32 ChunksRemainingInSound = RoundReal32ToInt32(RealChunksRemainingInSound);
                if (ChunksToMix > ChunksRemainingInSound) {
                    ChunksToMix = ChunksRemainingInSound;
                }
#define OutputChannelCount 2
                bool32 VolumeEnded[OutputChannelCount] = {};
                for (uint32 ChannelIndex = 0; ChannelIndex < OutputChannelCount; ++ChannelIndex) {
                    if (dVolumeChunk.E[ChannelIndex] != 0.0f) {
                        real32 DeltaVolume =
                            PlayingSound->TargetVolume.E[ChannelIndex] - Volume.E[ChannelIndex];
                        uint32 VolumeChunkCount = (uint32)(0.25f * (DeltaVolume / dVolumeChunk.E[ChannelIndex] + 0.5f));
                        if (ChunksToMix >= VolumeChunkCount) {
                            ChunksToMix = VolumeChunkCount;
                            VolumeEnded[ChannelIndex] = true;
                        }
                    }
                }

                real32 SamplePosition = PlayingSound->SamplesPlayed;
                for (uint32 LoopIndex = 0; LoopIndex < ChunksToMix; ++LoopIndex) {
#if 0
                    for (uint32 SampleOffset = 0; SampleOffset < 4; ++SampleOffset) {
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
                    __m128 SampleValue0 = _mm_setr_ps(
                        LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 0.0f * dSample)],
                        LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 1.0f * dSample)],
                        LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 2.0f * dSample)],
                        LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 3.0f * dSample)]
                    );

                    __m128 D0 = _mm_load_ps((real32*)&Dest0[0]);
                    __m128 D1 = _mm_load_ps((real32*)&Dest1[0]);

                    D0 = _mm_add_ps(D0, _mm_mul_ps(_mm_mul_ps(MasterVolume0_4x, Volume0_4x), SampleValue0));
                    D1 = _mm_add_ps(D1, _mm_mul_ps(_mm_mul_ps(MasterVolume1_4x, Volume1_4x), SampleValue0));

                    _mm_store_ps((real32*)&Dest0[0], D0);
                    _mm_store_ps((real32*)&Dest1[0], D1);

                    Dest0++;
                    Dest1++;
                    Volume0_4x = _mm_add_ps(Volume0_4x, dVolumeChunk0_4x);
                    Volume1_4x = _mm_add_ps(Volume1_4x, dVolumeChunk1_4x);
#endif
                    Volume += dVolumeChunk;
                    SamplePosition += dSampleChunk;
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
                Assert(TotalChunksToMix >= ChunksToMix);
                TotalChunksToMix -= ChunksToMix;

                if ((uint32)PlayingSound->SamplesPlayed >= LoadedSound->SampleCount) {
                    if (IsValid(Info->NextIDToPlay)) {
                        PlayingSound->ID = Info->NextIDToPlay;
                        PlayingSound->SamplesPlayed = (real32)LoadedSound->SampleCount;
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
            SampleIndex < ChunkCount;
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
