#if !defined(HANDMADE_ASSET)
#define HANDMADE_ASSET

#include "../types.h"
#include "math.cpp"
#include "bmp.cpp"
#include "memory.cpp"
#include "random.cpp"
#include "asset_type_id.h"

enum asset_tag_id {
    Tag_Smoothness,
    Tag_Flatness,
    Tag_FacingDirection, // Angle in radians off of due right
    Tag_Count
};

struct asset_tag {
    uint32 ID;
    real32 Value;
};

struct asset_bitmap_info {
    v2 AlignPercentage;
    char* Filename;
};

struct sound_id {
    uint32 Value;
};

struct asset_sound_info {
    char* Filename;
    uint32 FirstSampleIndex;
    uint32 SampleCount;
    sound_id NextIDToPlay;
};

struct asset {
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;

    union {
        asset_bitmap_info Bitmap;
        asset_sound_info Sound;
    };
};

struct asset_vector {
    real32 E[Tag_Count];
};

struct asset_type {
    uint32 FirstAssetIndex;
    uint32 OnePastLastAssetIndex;
};

enum asset_state {
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
    AssetState_Locked,
};

struct loaded_sound {
    uint32 SampleCount;
    uint32 ChannelCount;
    int16* Samples[2];
};

struct asset_slot {
    asset_state State;
    union {
        loaded_bitmap* Bitmap;
        loaded_sound* Sound;
    };
};


struct asset_group {
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
};

struct game_assets {
    struct transient_state* TranState;
    memory_arena Arena;

    real32 TagRange[Tag_Count];

    uint32 TagCount;
    asset_tag* Tags;

    uint32 AssetCount;
    asset* Assets;
    asset_slot* Slots;

    asset_type AssetTypes[Asset_Count];

    asset_type* DEBUGAssetType;
    uint32 DEBUGUsedAssetCount;
    uint32 DEBUGUsedTagCount;
    asset* DEBUGAsset;
};

struct bitmap_id {
    uint32 Value;
};

internal void SetTopDownAlign(loaded_bitmap* Bitmap, v2 Align) {
    Align = TopDownAlign(Bitmap, Align);
    Bitmap->AlignPercentage = Align;
}


internal void BeginAssetType(game_assets* Assets, asset_type_id Type) {
    Assert(Assets->DEBUGAssetType == 0);
    Assets->DEBUGAssetType = Assets->AssetTypes + Type;
    Assets->DEBUGAssetType->FirstAssetIndex = Assets->DEBUGUsedAssetCount;
    Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

internal bitmap_id
AddBitmapAsset(game_assets* Assets, char* Filename, v2 AlignPercentage = V2(0.5f, 0.5f)) {
    Assert(Assets->DEBUGAssetType != 0);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < Assets->AssetCount);
    bitmap_id Result = { Assets->DEBUGAssetType->OnePastLastAssetIndex++ };
    asset* Asset = Assets->Assets + Result.Value;
    Asset->FirstTagIndex = Assets->DEBUGUsedTagCount;
    Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
    Asset->Bitmap.Filename = Filename;
    Asset->Bitmap.AlignPercentage = AlignPercentage;
    Assets->DEBUGAsset = Asset;
    return Result;
}

internal sound_id
AddSoundAsset(game_assets* Assets, char* Filename, uint32 FirstSampleIndex = 0, uint32 SampleCount = 0) {
    Assert(Assets->DEBUGAssetType != 0);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < Assets->AssetCount);
    sound_id Result = { Assets->DEBUGAssetType->OnePastLastAssetIndex++ };
    asset* Asset = Assets->Assets + Result.Value;
    Asset->FirstTagIndex = Assets->DEBUGUsedTagCount;
    Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
    Asset->Sound.Filename = Filename;
    Asset->Sound.FirstSampleIndex = FirstSampleIndex;
    Asset->Sound.SampleCount = SampleCount;
    Asset->Sound.NextIDToPlay.Value = 0;
    Assets->DEBUGAsset = Asset;
    return Result;
}

internal void AddTag(game_assets* Assets, asset_tag_id TagID, real32 Value) {
    Assert(Assets->DEBUGAsset);
    ++Assets->DEBUGAsset->OnePastLastTagIndex;
    asset_tag* Tag = Assets->Tags + Assets->DEBUGUsedTagCount++;
    Tag->ID = TagID;
    Tag->Value = Value;
}

internal void EndAssetType(game_assets* Assets) {
    Assert(Assets->DEBUGAssetType != 0);
    Assets->DEBUGUsedAssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
    Assets->DEBUGAssetType = 0;
    Assets->DEBUGAsset = 0;
}

internal bool32 IsValid(sound_id ID) {
    bool32 Result = ID.Value != 0;
    return Result;
}

internal bool32 IsValid(bitmap_id ID) {
    bool32 Result = ID.Value != 0;
    return Result;
}

internal game_assets*
AllocateGameAssets(memory_arena* Arena, memory_index Size, transient_state* TranState) {
    game_assets* Assets = PushStruct(Arena, game_assets);
    SubArena(&Assets->Arena, Arena, Size);
    Assets->TranState = TranState;

    for (uint32 TagType = 0; TagType < Tag_Count; ++TagType) {
        Assets->TagRange[TagType] = 100000.0f;
    }
    Assets->TagRange[Tag_FacingDirection] = 2.0f * Pi32;

    Assets->TagCount = 1024 * Asset_Count;
    Assets->Tags = PushArray(Arena, Assets->TagCount, asset_tag);

    Assets->AssetCount = 2 * 256 * Asset_Count;
    Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);
    Assets->Slots = PushArray(Arena, Assets->AssetCount, asset_slot);

    Assets->DEBUGUsedAssetCount = 1;

    BeginAssetType(Assets, Asset_Shadow);
    AddBitmapAsset(Assets, "test/test_hero_shadow.bmp", V2(0.5f, 0.15668f));
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Tree);
    AddBitmapAsset(Assets, "test2/tree00.bmp", V2(0.494f, 0.295f));
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Sword);
    AddBitmapAsset(Assets, "test2/rock03.bmp", V2(0.5f, 0.656f));
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Grass);
    AddBitmapAsset(Assets, "test2/grass00.bmp");
    AddBitmapAsset(Assets, "test2/grass01.bmp");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Tuft);
    AddBitmapAsset(Assets, "test2/tuft00.bmp");
    AddBitmapAsset(Assets, "test2/tuft01.bmp");
    AddBitmapAsset(Assets, "test2/tuft02.bmp");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Stone);
    AddBitmapAsset(Assets, "test2/ground00.bmp");
    AddBitmapAsset(Assets, "test2/ground01.bmp");
    AddBitmapAsset(Assets, "test2/ground02.bmp");
    AddBitmapAsset(Assets, "test2/ground03.bmp");
    EndAssetType(Assets);

    real32 AngleRight = 0.0f;
    real32 AngleBack = 0.5f * Pi32;
    real32 AngleLeft = Pi32;
    real32 AngleFront = 1.5f * Pi32;

    v2 HeroAlign = V2(0.5f, 0.1567);

    BeginAssetType(Assets, Asset_Head);
    AddBitmapAsset(Assets, "test/test_hero_right_head.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/test_hero_back_head.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/test_hero_left_head.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/test_hero_front_head.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Cape);
    AddBitmapAsset(Assets, "test/test_hero_right_cape.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/test_hero_back_cape.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/test_hero_left_cape.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/test_hero_front_cape.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Torso);
    AddBitmapAsset(Assets, "test/test_hero_right_torso.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/test_hero_back_torso.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/test_hero_left_torso.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/test_hero_front_torso.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    // Sounds

    BeginAssetType(Assets, Asset_Bloop);
    AddSoundAsset(Assets, "test3/bloop_00.wav");
    AddSoundAsset(Assets, "test3/bloop_01.wav");
    AddSoundAsset(Assets, "test3/bloop_02.wav");
    AddSoundAsset(Assets, "test3/bloop_03.wav");
    AddSoundAsset(Assets, "test3/bloop_04.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Crack);
    AddSoundAsset(Assets, "test3/crack_00.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Drop);
    AddSoundAsset(Assets, "test3/drop_00.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Glide);
    AddSoundAsset(Assets, "test3/glide_00.wav");
    EndAssetType(Assets);

    uint32 OneMusicChunk = 10 * 48000;
    uint32 TotalMusicSampleCount = 7468095;
    BeginAssetType(Assets, Asset_Music);
    sound_id LastMusic = { 0 };
    for (uint32 FirstSampleIndex = 0; FirstSampleIndex < TotalMusicSampleCount; FirstSampleIndex += OneMusicChunk) {
        uint32 SampleCount = TotalMusicSampleCount - FirstSampleIndex;
        if (SampleCount > OneMusicChunk) {
            SampleCount = OneMusicChunk;
        }
        sound_id ThisMusic = AddSoundAsset(Assets, "test3/music_test.wav", FirstSampleIndex, SampleCount);
        if (IsValid(LastMusic)) {
            Assets->Assets[LastMusic.Value].Sound.NextIDToPlay = ThisMusic;
        }
        LastMusic = ThisMusic;
    }
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Puhp);
    AddSoundAsset(Assets, "test3/puhp_00.wav");
    AddSoundAsset(Assets, "test3/puhp_01.wav");
    EndAssetType(Assets);

    return Assets;
}

internal inline loaded_bitmap* GetBitmap(game_assets* Assets, bitmap_id ID) {
    Assert(ID.Value <= Assets->AssetCount);
    loaded_bitmap* Result = Assets->Slots[ID.Value].Bitmap;
    return Result;
}

internal inline loaded_sound* GetSound(game_assets* Assets, sound_id ID) {
    Assert(ID.Value <= Assets->AssetCount);
    loaded_sound* Result = Assets->Slots[ID.Value].Sound;
    return Result;
}

internal inline asset_sound_info* GetSoundInfo(game_assets* Assets, sound_id ID) {
    Assert(ID.Value <= Assets->AssetCount);
    asset_sound_info* Result = &Assets->Assets[ID.Value].Sound;
    return Result;
}

internal uint32 GetBestMatchAssetFrom(
    game_assets* Assets, asset_type_id TypeID,
    asset_vector* MatchVector, asset_vector* WeightVector
) {
    real32 BestDiff = Real32Maximum;
    uint32 Result = 0;
    asset_type* Type = Assets->AssetTypes + TypeID;
    for (uint32 AssetIndex = Type->FirstAssetIndex;
        AssetIndex < Type->OnePastLastAssetIndex;
        ++AssetIndex) {
        asset* Asset = Assets->Assets + AssetIndex;
        real32 TotalWeightedDiff = 0;
        for (uint32 TagIndex = Asset->FirstTagIndex;
            TagIndex < Asset->OnePastLastTagIndex;
            ++TagIndex) {
            asset_tag* Tag = Assets->Tags + TagIndex;
            real32 A = MatchVector->E[Tag->ID];
            real32 B = Tag->Value;
            real32 D0 = AbsoluteValue(A - B);
            real32 D1 = AbsoluteValue(A - Assets->TagRange[Tag->ID] * SignOf(A) - B);
            real32 Difference = Minimum(D0, D1);
            real32 Weighted = WeightVector->E[Tag->ID] * AbsoluteValue(Difference);
            TotalWeightedDiff += Weighted;
        }
        if (BestDiff > TotalWeightedDiff) {
            BestDiff = TotalWeightedDiff;
            Result = AssetIndex;
        }
    }
    return Result;
}

internal bitmap_id GetBestMatchBitmapFrom(
    game_assets* Assets, asset_type_id TypeID,
    asset_vector* MatchVector, asset_vector* WeightVector
) {
    bitmap_id Result = { GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector) };
    return Result;
}

internal sound_id GetBestMatchSoundFrom(
    game_assets* Assets, asset_type_id TypeID,
    asset_vector* MatchVector, asset_vector* WeightVector
) {
    sound_id Result = { GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector) };
    return Result;
}

internal uint32
GetRandomSlotFrom(game_assets* Assets, asset_type_id TypeID, random_series* Series) {
    uint32 Result = 0;
    asset_type* Type = Assets->AssetTypes + TypeID;
    if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex) {
        uint32 Count = Type->OnePastLastAssetIndex - Type->FirstAssetIndex;
        uint32 Choice = RandomChoice(Series, Count);
        Result = Type->FirstAssetIndex + Choice;
    }
    return Result;
}

internal bitmap_id
GetRandomBitmapFrom(game_assets* Assets, asset_type_id TypeID, random_series* Series) {
    bitmap_id Result = { GetRandomSlotFrom(Assets, TypeID, Series) };
    return Result;
}

internal sound_id
GetRandomSoundFrom(game_assets* Assets, asset_type_id TypeID, random_series* Series) {
    sound_id Result = { GetRandomSlotFrom(Assets, TypeID, Series) };
    return Result;
}

internal uint32 GetFirstAssetFrom(game_assets* Assets, asset_type_id TypeID) {
    uint32 Result = 0;
    asset_type* Type = Assets->AssetTypes + TypeID;
    if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex) {
        Result = Type->FirstAssetIndex;
    }
    return Result;
}

internal bitmap_id GetFirstBitmapFrom(game_assets* Assets, asset_type_id TypeID) {
    bitmap_id Result = { GetFirstAssetFrom(Assets, TypeID) };
    return Result;
}

internal sound_id GetFirstSoundFrom(game_assets* Assets, asset_type_id TypeID) {
    sound_id Result = { GetFirstAssetFrom(Assets, TypeID) };
    return Result;
}

#pragma pack(push, 1)
struct WAVE_header {
    uint32 RIFFID;
    uint32 Size; // NOTE(sen) Includes the WAVEID below
    uint32 WAVEID;
};
#define RIFF_CODE(a,b,c,d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))
enum WAVE_codes {
    WAVE_ChunkID_fmt = RIFF_CODE('f', 'm', 't', ' '),
    WAVE_ChunkID_data = RIFF_CODE('d', 'a', 't', 'a'),
    WAVE_ChunkID_RIFF = RIFF_CODE('R', 'I', 'F', 'F'),
    WAVE_ChunkID_WAVE = RIFF_CODE('W', 'A', 'V', 'E')
};
struct WAVE_chunk {
    uint32 ID;
    uint32 Size;
};
struct WAVE_fmt {
    uint16 wFormatTag;
    uint16 nChannels;
    uint32 nSamplesPerSec;
    uint32 nAvgBytesPerSec;
    uint16 nBlockAlign;
    uint16 wBitsPerSample;
    uint16 cbSize;
    uint16 wValidBitsPerSample;
    uint32 dwChannelMask;
    uint8 SubFormat[16];
};
#pragma pack(pop)

struct riff_iterator {
    uint8* At;
    uint8* Stop;
};

internal riff_iterator ParseChunk(void* At, void* Stop) {
    riff_iterator Iter;
    Iter.At = (uint8*)At;
    Iter.Stop = (uint8*)Stop;
    return Iter;
}

internal bool32 IsValid(riff_iterator Iter) {
    bool32 Result = Iter.At < Iter.Stop;
    return Result;
}

internal riff_iterator NextChunk(riff_iterator Iter) {
    WAVE_chunk* Chunk = (WAVE_chunk*)Iter.At;
    uint32 Size = (Chunk->Size + 1) & ~1;
    Iter.At += sizeof(WAVE_chunk) + Size;
    return Iter;
}

internal void* GetChunkData(riff_iterator Iter) {
    void* Result = Iter.At + sizeof(WAVE_chunk);
    return Result;
}

internal uint32 GetChunkDataSize(riff_iterator Iter) {
    WAVE_chunk* Chunk = (WAVE_chunk*)Iter.At;
    uint32 Result = Chunk->Size;
    return Result;
}

internal uint32 GetType(riff_iterator Iter) {
    WAVE_chunk* Chunk = (WAVE_chunk*)Iter.At;
    uint32 Result = Chunk->ID;
    return Result;
}

internal loaded_sound
DEBUGLoadWAV(char* Filename, uint32 SectionFirstSampleIndex, uint32 SectionSampleCount) {
    loaded_sound Result = {};
    debug_read_file_result ReadResult = DEBUGPlatformReadEntireFile(Filename);
    if (ReadResult.Size != 0) {
        WAVE_header* Header = (WAVE_header*)ReadResult.Contents;
        Assert(Header->RIFFID == WAVE_ChunkID_RIFF);
        Assert(Header->WAVEID == WAVE_ChunkID_WAVE);
        uint32 ChannelCount = 0;
        uint32 SampleDataSize = 0;
        int16* SampleData = 0;
        for (riff_iterator Iter = ParseChunk(Header + 1, (uint8*)(Header + 1) + Header->Size - 4);
            IsValid(Iter);
            Iter = NextChunk(Iter)) {
            switch (GetType(Iter)) {
            case WAVE_ChunkID_fmt: {
                WAVE_fmt* fmt = (WAVE_fmt*)GetChunkData(Iter);
                Assert(fmt->wFormatTag == 1); // PCM
                Assert(fmt->nSamplesPerSec == 48000);
                Assert(fmt->wBitsPerSample == 16);
                Assert(fmt->nBlockAlign == fmt->nChannels * sizeof(int16));
                ChannelCount = fmt->nChannels;
            } break;
            case WAVE_ChunkID_data: {
                SampleData = (int16*)GetChunkData(Iter);
                SampleDataSize = GetChunkDataSize(Iter);
            } break;
            }
        }
        Assert(ChannelCount && SampleData && SampleDataSize);
        Result.ChannelCount = ChannelCount;
        uint32 SampleCount = SampleDataSize / (ChannelCount * sizeof(int16));
        if (ChannelCount == 1) {
            Result.Samples[0] = SampleData;
            Result.Samples[1] = 0;
        } else if (ChannelCount == 2) {
            Result.Samples[0] = SampleData;
            Result.Samples[1] = SampleData + SampleCount;
#if 0
            for (uint32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex) {
                SampleData[2 * SampleIndex + 0] = (int16)SampleIndex;
                SampleData[2 * SampleIndex + 1] = (int16)SampleIndex;
            }
#endif
            for (uint32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex) {
                uint16 Source = SampleData[2 * SampleIndex];
                SampleData[2 * SampleIndex] = SampleData[SampleIndex];
                SampleData[SampleIndex] = Source;
            }
        } else {
            Assert(!"Invalid channel count");
        }

        Result.ChannelCount = 1;

        bool32 AtEnd = true;
        if (SectionSampleCount) {
            Assert(SectionFirstSampleIndex + SectionSampleCount <= SampleCount);
            AtEnd = (SectionFirstSampleIndex + SectionSampleCount) == SampleCount;
            SampleCount = SectionSampleCount;
            for (uint32 ChannelIndex = 0; ChannelIndex < Result.ChannelCount; ++ChannelIndex) {
                Result.Samples[ChannelIndex] += SectionFirstSampleIndex;
            }
        }
        if (AtEnd) {
            uint32 SampleCountAlign8 = SampleCount + 8; // Align8(SampleCount);
            for (uint32 ChannelIndex = 0; ChannelIndex < Result.ChannelCount; ++ChannelIndex) {
                for (uint32 SampleIndex = SampleCount; SampleIndex < SampleCountAlign8; ++SampleIndex) {
                    Result.Samples[ChannelIndex][SampleIndex] = 0;
                }
            }
        }
        Result.SampleCount = SampleCount;
    }
    return Result;
}

#endif
