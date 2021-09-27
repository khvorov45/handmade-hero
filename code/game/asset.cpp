#if !defined(HANDMADE_ASSET)
#define HANDMADE_ASSET

#include "lib.hpp"
#include "../types.h"
#include "../intrinsics.h"
#include "math.cpp"
#include "bmp.cpp"
#include "memory.cpp"
#include "random.cpp"
#include "asset_type_id.h"
#include "../file_formats.h"

struct asset_bitmap_info {
    v2 AlignPercentage;
    char* Filename;
};

struct asset_sound_info {
    char* Filename;
    uint32 FirstSampleIndex;
    uint32 SampleCount;
    sound_id NextIDToPlay;
};

struct asset_vector {
    real32 E[Tag_Count];
};

struct asset_type {
    uint32 FirstAssetIndex;
    uint32 OnePastLastAssetIndex;
};

struct loaded_sound {
    int16* Samples[2];
    uint32 SampleCount;
    uint32 ChannelCount;
};

struct asset_memory_header {
    asset_memory_header* Next;
    asset_memory_header* Prev;
    uint32 AssetIndex;
    uint32 TotalSize;
    union {
        loaded_bitmap Bitmap;
        loaded_sound Sound;
    };
};

enum asset_state {
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,

    AssetState_StateMask = 0xFFF,

    AssetState_Lock = 0x10000,
};

struct asset_memory_size {
    uint32 Total;
    uint32 Data;
    uint32 Section;
};

struct asset {
    uint32 State;
    asset_memory_header* Header;
    hha_asset HHA;
    uint32 FileIndex;
};

struct asset_group {
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
};

struct asset_file {
    platform_file_handle* Handle;
    hha_header Header;
    hha_asset_type* AssetTypeArray;
    uint32 TagBase;
};

enum asset_memory_block_flags {
    AssetMemory_Used = 0x1,
};

struct asset_memory_block {
    asset_memory_block* Prev;
    asset_memory_block* Next;
    uint64 Flags;
    memory_index Size;
};

struct game_assets {
    struct transient_state* TranState;

    asset_memory_block MemorySentinel;

    uint64 TotalMemoryUsed;
    uint64 TargetMemoryUsed;
    asset_memory_header LoadedAssetSentinel;

    real32 TagRange[Tag_Count];

    uint32 FileCount;
    asset_file* Files;

    uint32 TagCount;
    hha_tag* Tags;

    uint32 AssetCount;
    asset* Assets;

    asset_type AssetTypes[Asset_Count];

    // uint8* HHAContents;
};

#if 0
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
    hha_asset* Asset = Assets->Assets + Result.Value;
    Asset->FirstTagIndex = Assets->DEBUGUsedTagCount;
    Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
    Asset->Bitmap.Filename = PushString(&Assets->Arena, Filename);;
    Asset->Bitmap.AlignPercentage = AlignPercentage;
    Assets->DEBUGAsset = Asset;
    return Result;
}

internal sound_id
AddSoundAsset(game_assets* Assets, char* Filename, uint32 FirstSampleIndex = 0, uint32 SampleCount = 0) {
    Assert(Assets->DEBUGAssetType != 0);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < Assets->AssetCount);
    sound_id Result = { Assets->DEBUGAssetType->OnePastLastAssetIndex++ };
    hha_asset* Asset = Assets->Assets + Result.Value;
    Asset->FirstTagIndex = Assets->DEBUGUsedTagCount;
    Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
    Asset->Sound.Filename = PushString(&Assets->Arena, Filename);
    Asset->Sound.FirstSampleIndex = FirstSampleIndex;
    Asset->Sound.SampleCount = SampleCount;
    Asset->Sound.NextIDToPlay.Value = 0;
    Assets->DEBUGAsset = Asset;
    return Result;
}

internal void AddTag(game_assets* Assets, hha_tag_id TagID, real32 Value) {
    Assert(Assets->DEBUGAsset);
    ++Assets->DEBUGAsset->OnePastLastTagIndex;
    hha_tag* Tag = Assets->Tags + Assets->DEBUGUsedTagCount++;
    Tag->ID = TagID;
    Tag->Value = Value;
}

internal void EndAssetType(game_assets* Assets) {
    Assert(Assets->DEBUGAssetType != 0);
    Assets->DEBUGUsedAssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
    Assets->DEBUGAssetType = 0;
    Assets->DEBUGAsset = 0;
}
#endif

internal bool32 IsValid(sound_id ID) {
    bool32 Result = ID.Value != 0;
    return Result;
}

internal bool32 IsValid(bitmap_id ID) {
    bool32 Result = ID.Value != 0;
    return Result;
}

internal bool32 IsLocked(asset* Asset) {
    bool32 Result = (Asset->State & AssetState_Lock) != 0;
    return Result;
}

internal asset_memory_block* InsertBlock(asset_memory_block* Prev, memory_index Size, void* Memory) {
    Assert(Size > sizeof(asset_memory_block));
    asset_memory_block* Block = (asset_memory_block*)Memory;
    Block->Flags = 0;
    Block->Size = Size - sizeof(asset_memory_block);
    Block->Prev = Prev;
    Block->Next = Prev->Next;
    Block->Prev->Next = Block;
    Block->Next->Prev = Block;
    return Block;
}

internal game_assets*
AllocateGameAssets(memory_arena* Arena, memory_index Size, transient_state* TranState) {
    game_assets* Assets = PushStruct(Arena, game_assets);

    Assets->MemorySentinel.Flags = 0;
    Assets->MemorySentinel.Size = 0;
    Assets->MemorySentinel.Prev = &Assets->MemorySentinel;
    Assets->MemorySentinel.Next = &Assets->MemorySentinel;

    InsertBlock(&Assets->MemorySentinel, Size, PushSize(Arena, Size));

    Assets->TranState = TranState;
    Assets->TotalMemoryUsed = 0;
    Assets->TargetMemoryUsed = Size;

    Assets->LoadedAssetSentinel.Next = &Assets->LoadedAssetSentinel;
    Assets->LoadedAssetSentinel.Prev = &Assets->LoadedAssetSentinel;

    for (uint32 TagType = 0; TagType < Tag_Count; ++TagType) {
        Assets->TagRange[TagType] = 100000.0f;
    }
    Assets->TagRange[Tag_FacingDirection] = 2.0f * Pi32;

#if 1
    Assets->TagCount = 1;
    Assets->AssetCount = 1;
    {
        platform_file_group* FileGroup = Platform.GetAllFilesOfTypeBegin("hha");
        Assets->FileCount = FileGroup->FileCount;
        Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
        for (uint32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex) {

            asset_file* File = Assets->Files + FileIndex;

            File->TagBase = Assets->TagCount;

            ZeroStruct(File->Header);
            File->Handle = Platform.OpenNextFile(FileGroup);
            Platform.ReadDataFromFile(File->Handle, 0, sizeof(File->Header), &File->Header);

            uint32 AssetTypeArraySize = File->Header.AssetTypeCount * sizeof(hha_asset_type);
            File->AssetTypeArray = (hha_asset_type*)PushSize(Arena, AssetTypeArraySize);
            Platform.ReadDataFromFile(
                File->Handle,
                File->Header.AssetTypes,
                AssetTypeArraySize,
                File->AssetTypeArray
            );
            if (File->Header.MagicValue != HHA_MAGIC_VALUE) {
                Platform.FileError(File->Handle, "HHA File has an invalid magic value");
            }
            if (File->Header.Version > HHA_VERSION) {
                Platform.FileError(File->Handle, "HHA file is of a later version");
            }
            if (PlatformNoFileErrors(File->Handle)) {
                Assets->TagCount += (File->Header.TagCount - 1);
                Assets->AssetCount += (File->Header.AssetCount - 1);
            } else {
                InvalidCodePath
            }
        }
        Platform.GetAllFilesOfTypeEnd(FileGroup);
    }

    Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);
    Assets->Tags = PushArray(Arena, Assets->TagCount, hha_tag);

    ZeroStruct(Assets->Tags[0]);

    // Tags
    for (uint32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex) {
        asset_file* File = Assets->Files + FileIndex;
        if (PlatformNoFileErrors(File->Handle)) {
            uint32 TagArraySize = sizeof(hha_tag) * (File->Header.TagCount - 1);
            Platform.ReadDataFromFile(
                File->Handle,
                File->Header.Tags + sizeof(hha_tag),
                TagArraySize,
                Assets->Tags + File->TagBase
            );
        }
    }

    uint32 AssetCount = 0;
    ZeroStruct(*(Assets->Assets + AssetCount));
    ++AssetCount;
    for (uint32 DestTypeID = 0; DestTypeID < Asset_Count; ++DestTypeID) {
        asset_type* DestType = Assets->AssetTypes + DestTypeID;
        DestType->FirstAssetIndex = AssetCount;
        for (uint32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex) {
            asset_file* File = Assets->Files + FileIndex;
            if (PlatformNoFileErrors(File->Handle)) {
                for (uint32 SourceIndex = 0; SourceIndex < File->Header.AssetTypeCount; ++SourceIndex) {
                    hha_asset_type* SourceType = File->AssetTypeArray + SourceIndex;
                    if (SourceType->TypeID == DestTypeID) {
                        uint32 AssetCountForType = SourceType->OnePastLastAssetIndex - SourceType->FirstAssetIndex;
                        temporary_memory TempMem = BeginTemporaryMemory(Arena);
                        hha_asset* HHAAssetArray = PushArray(TempMem.Arena, AssetCountForType, hha_asset);
                        Platform.ReadDataFromFile(
                            File->Handle,
                            File->Header.Assets + SourceType->FirstAssetIndex * sizeof(hha_asset),
                            sizeof(hha_asset) * AssetCountForType,
                            HHAAssetArray
                        );
                        for (uint32 AssetIndex = 0; AssetIndex < AssetCountForType; ++AssetIndex) {
                            hha_asset* HHAAsset = HHAAssetArray + AssetIndex;
                            Assert(AssetCount < Assets->AssetCount);
                            asset* Asset = Assets->Assets + AssetCount++;
                            Asset->FileIndex = FileIndex;
                            Asset->HHA = *HHAAsset;
                            if (Asset->HHA.FirstTagIndex == 0) {
                                Asset->HHA.FirstTagIndex = Asset->HHA.OnePastLastTagIndex = 0;
                            } else {
                                Asset->HHA.FirstTagIndex += File->TagBase - 1;
                                Asset->HHA.OnePastLastTagIndex += File->TagBase - 1;
                            }
                        }
                        EndTemporaryMemory(TempMem);
                    }
                }
            }
        }
        DestType->OnePastLastAssetIndex = AssetCount;
    }
    Assert(AssetCount == Assets->AssetCount);

#else

    debug_read_file_result ReadResult = Platform.DEBUGReadEntireFile("test.hha");
    if (ReadResult.Size != 0) {
        hha_header* Header = (hha_header*)ReadResult.Contents;
        Assert(Header->MagicValue == HHA_MAGIC_VALUE);
        Assert(Header->Version == HHA_VERSION);

        Assets->AssetCount = Header->AssetCount;
        Assets->Assets = (hha_asset*)((uint8*)ReadResult.Contents + Header->Assets);
        Assets->Assets = PushArray(Arena, Assets->AssetCount, asset_slot);

        Assets->TagCount = Header->TagCount;
        Assets->Tags = (hha_tag*)((uint8*)ReadResult.Contents + Header->Tags);

        hha_asset_type* HHAAssetTypes = (hha_asset_type*)((uint8*)ReadResult.Contents + Header->AssetTypes);

        for (uint32 Index = 0; Index < Header->AssetTypeCount; ++Index) {
            hha_asset_type* Source = HHAAssetTypes + Index;
            if (Source->TypeID < Asset_Count) {
                asset_type* Dest = Assets->AssetTypes + Source->TypeID;
                Assert(Dest->FirstAssetIndex == 0);
                Assert(Dest->OnePastLastAssetIndex == 0);
                Dest->FirstAssetIndex = Source->FirstAssetIndex;
                Dest->OnePastLastAssetIndex = Source->OnePastLastAssetIndex;
            }
        }

        Assets->HHAContents = (uint8*)ReadResult.Contents;
    }
#endif

#if 0
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
#endif
    return Assets;
}

internal uint32 GetState(asset* Asset) {
    uint32 Result = Asset->State & AssetState_StateMask;
    return Result;
}

internal void InsertAssetheaderAtFront(game_assets* Assets, asset_memory_header* Header) {
    Header->Prev = &Assets->LoadedAssetSentinel;
    Header->Next = Assets->LoadedAssetSentinel.Next;
    Header->Prev->Next = Header;
    Header->Next->Prev = Header;
}

internal void RemoveAssetHeaderFromList(asset_memory_header* Header) {
    Header->Prev->Next = Header->Next;
    Header->Next->Prev = Header->Prev;
    Header->Next = Header->Prev = 0;
}

internal asset_memory_size GetSizeOfAsset(game_assets* Assets, bool32 IsSound, uint32 AssetIndex) {
    asset* Asset = Assets->Assets + AssetIndex;
    asset_memory_size Result = {};
    if (IsSound) {
        hha_sound* Sound = &Asset->HHA.Sound;
        Result.Section = Sound->SampleCount * sizeof(int16);
        Result.Data = Sound->ChannelCount * Result.Section;
    } else {
        hha_bitmap* Info = &Asset->HHA.Bitmap;
        uint32 Width = Info->Dim[0];
        uint32 Height = Info->Dim[1];
        Result.Section = Width * 4;
        Result.Data = Result.Section * Height;
    }
    Result.Total = Result.Data + sizeof(asset_memory_header);
    return Result;
}

internal void MoveHeaderToFront(game_assets* Assets, asset* Asset) {
    if (!IsLocked(Asset)) {
        asset_memory_header* Header = Asset->Header;
        RemoveAssetHeaderFromList(Header);
        InsertAssetheaderAtFront(Assets, Header);
    }
}

internal inline loaded_bitmap* GetBitmap(game_assets* Assets, bitmap_id ID, bool32 MustBeLocked) {
    Assert(ID.Value <= Assets->AssetCount);
    asset* Asset = Assets->Assets + ID.Value;
    loaded_bitmap* Result = 0;
    if (GetState(Asset) >= AssetState_Loaded) {
        Assert(!MustBeLocked || IsLocked(Asset));
        CompletePreviousReadsBeforeFutureReads;
        Result = &Asset->Header->Bitmap;
        MoveHeaderToFront(Assets, Asset);
    }
    return Result;
}

internal inline loaded_sound* GetSound(game_assets* Assets, sound_id ID) {
    Assert(ID.Value <= Assets->AssetCount);
    asset* Asset = Assets->Assets + ID.Value;
    loaded_sound* Result = 0;
    if (GetState(Asset) >= AssetState_Loaded) {
        CompletePreviousReadsBeforeFutureReads;
        Result = &Asset->Header->Sound;
        MoveHeaderToFront(Assets, Asset);
    }
    return Result;
}

internal inline hha_sound* GetSoundInfo(game_assets* Assets, sound_id ID) {
    Assert(ID.Value <= Assets->AssetCount);
    hha_sound* Result = &Assets->Assets[ID.Value].HHA.Sound;
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
        hha_asset* Asset = &Assets->Assets[AssetIndex].HHA;
        real32 TotalWeightedDiff = 0;
        for (uint32 TagIndex = Asset->FirstTagIndex;
            TagIndex < Asset->OnePastLastTagIndex;
            ++TagIndex) {
            hha_tag* Tag = Assets->Tags + TagIndex;
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
GetRandomAssetFrom(game_assets* Assets, asset_type_id TypeID, random_series* Series) {
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
    bitmap_id Result = { GetRandomAssetFrom(Assets, TypeID, Series) };
    return Result;
}

internal sound_id
GetRandomSoundFrom(game_assets* Assets, asset_type_id TypeID, random_series* Series) {
    sound_id Result = { GetRandomAssetFrom(Assets, TypeID, Series) };
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

internal sound_id GetNextSoundInChain(game_assets* Assets, sound_id SoundID) {
    sound_id Result = {};
    hha_sound* Info = GetSoundInfo(Assets, SoundID);
    switch (Info->Chain) {
    case HHASoundChain_None: break;
    case HHASoundChain_Advance: Result.Value = SoundID.Value + 1; break;
    case HHASoundChain_Loop: Result.Value = SoundID.Value; break;
        InvalidDefaultCase;
    }
    return Result;
}

#if 0

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

internal void ReleaseAssetMemory(game_assets* Assets, memory_index Size, void* Memory) {
    if (Memory) {
        Assets->TotalMemoryUsed -= Size;
    }
#if 0
    Platform.DeallocateMemory(Memory);
#else
    asset_memory_block* Block = (asset_memory_block*)Memory - 1;
    Block->Flags &= ~AssetMemory_Used;
#endif

}

internal void EvictAsset(game_assets* Assets, asset_memory_header* Header) {
    asset* Asset = Assets->Assets + Header->AssetIndex;
    Assert(GetState(Asset) == AssetState_Loaded);
    Assert(!IsLocked(Asset));
    RemoveAssetHeaderFromList(Header);
    ReleaseAssetMemory(Assets, Header->TotalSize, Asset->Header);
    Asset->State = AssetState_Unloaded;
    Asset->Header = 0;
}

internal void EvictAssetsAsNecessary(game_assets* Assets) {
#if 1
    while (Assets->TotalMemoryUsed > Assets->TargetMemoryUsed) {
        asset_memory_header* Header = Assets->LoadedAssetSentinel.Prev;
        if (Header != &Assets->LoadedAssetSentinel) {
            asset* Asset = Assets->Assets + Header->AssetIndex;
            if (GetState(Asset) >= AssetState_Loaded) {
                EvictAsset(Assets, Header);
            }
        } else {
            InvalidCodePath;
            break;
        }
    }
#endif
}

asset_memory_block* FindBlockForSize(game_assets* Assets, memory_index Size) {
    asset_memory_block* Result = 0;
    for (asset_memory_block* Block = Assets->MemorySentinel.Next; Block != &Assets->MemorySentinel; Block = Block->Next) {
        if (!(Block->Flags & AssetMemory_Used) && Block->Size >= Size) {
            Result = Block;
            break;
        }
    }
    return Result;
}

internal void* AcquireAssetMemory(game_assets* Assets, memory_index Size) {
    void* Result = 0;
#if 0
    EvictAssetsAsNecessary(Assets);
    Result = Platform.AllocateMemory(Size);
#else
    for (;;) {
        asset_memory_block* Block = FindBlockForSize(Assets, Size);
        if (Block) {
            Block->Flags |= AssetMemory_Used;

            Assert(Size <= Block->Size);
            Result = (uint8*)(Block + 1);

            memory_index RemainingSize = Block->Size - Size;
            memory_index BlockSplitThreshold = 4096;
            if (RemainingSize > BlockSplitThreshold) {
                Block->Size -= RemainingSize;
                InsertBlock(Block, RemainingSize, (uint8*)Result + Size);
            }

            break;
        } else {
            for (asset_memory_header* Header = Assets->LoadedAssetSentinel.Prev;
                Header != &Assets->LoadedAssetSentinel;
                Header = Header->Prev) {

                asset* Asset = Assets->Assets + Header->AssetIndex;
                if (GetState(Asset) >= AssetState_Loaded) {
                    EvictAsset(Assets, Header);
                    break;
                }
            }
        }
    }
#endif
    if (Result) {
        Assets->TotalMemoryUsed += Size;
    }
    return Result;
}

internal void AddAssetHeaderToList(game_assets* Assets, uint32 AssetIndex, asset_memory_size Size) {
    asset_memory_header* Header = Assets->Assets[AssetIndex].Header;
    Header->AssetIndex = AssetIndex;
    Header->TotalSize = Size.Total;
    InsertAssetheaderAtFront(Assets, Header);
}

#endif
