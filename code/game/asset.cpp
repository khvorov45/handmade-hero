#if !defined(HANDMADE_ASSET)
#define HANDMADE_ASSET

#include "lib.hpp"
#include "../types.h"
#include "../intrinsics.h"
#include "math.cpp"
#include "bmp.cpp"
#include "memory.cpp"
#include "random.cpp"
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
    uint32 GenerationID;
    union {
        loaded_bitmap Bitmap;
        loaded_sound Sound;
    };
};

enum asset_state {
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
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
    platform_file_handle Handle;
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
    uint32 NextGenerationID;

    struct transient_state* TranState;

    asset_memory_block MemorySentinel;

    asset_memory_header LoadedAssetSentinel;

    real32 TagRange[Tag_Count];

    uint32 FileCount;
    asset_file* Files;

    uint32 TagCount;
    hha_tag* Tags;

    uint32 AssetCount;
    asset* Assets;

    asset_type AssetTypes[Asset_Count];

    uint32 OperationLock;

    uint32 InFlightGenerationCount;
    uint32 InFlightGenerations[16];
};

internal bool32 IsValid(sound_id ID) {
    bool32 Result = ID.Value != 0;
    return Result;
}

internal bool32 IsValid(bitmap_id ID) {
    bool32 Result = ID.Value != 0;
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

    Assets->NextGenerationID = 0;
    Assets->InFlightGenerationCount = 0;

    Assets->MemorySentinel.Flags = 0;
    Assets->MemorySentinel.Size = 0;
    Assets->MemorySentinel.Prev = &Assets->MemorySentinel;
    Assets->MemorySentinel.Next = &Assets->MemorySentinel;

    InsertBlock(&Assets->MemorySentinel, Size, PushSize(Arena, Size));

    Assets->TranState = TranState;

    Assets->LoadedAssetSentinel.Next = &Assets->LoadedAssetSentinel;
    Assets->LoadedAssetSentinel.Prev = &Assets->LoadedAssetSentinel;

    for (uint32 TagType = 0; TagType < Tag_Count; ++TagType) {
        Assets->TagRange[TagType] = 100000.0f;
    }
    Assets->TagRange[Tag_FacingDirection] = 2.0f * Pi32;

    Assets->TagCount = 1;
    Assets->AssetCount = 1;
    {
        platform_file_group FileGroup = Platform.GetAllFilesOfTypeBegin(PlatformFileType_AssetFile);
        Assets->FileCount = FileGroup.FileCount;
        Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
        for (uint32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex) {

            asset_file* File = Assets->Files + FileIndex;

            File->TagBase = Assets->TagCount;

            ZeroStruct(File->Header);
            File->Handle = Platform.OpenNextFile(&FileGroup);
            Platform.ReadDataFromFile(&File->Handle, 0, sizeof(File->Header), &File->Header);

            uint32 AssetTypeArraySize = File->Header.AssetTypeCount * sizeof(hha_asset_type);
            File->AssetTypeArray = (hha_asset_type*)PushSize(Arena, AssetTypeArraySize);
            Platform.ReadDataFromFile(
                &File->Handle,
                File->Header.AssetTypes,
                AssetTypeArraySize,
                File->AssetTypeArray
            );
            if (File->Header.MagicValue != HHA_MAGIC_VALUE) {
                Platform.FileError(&File->Handle, "HHA File has an invalid magic value");
            }
            if (File->Header.Version > HHA_VERSION) {
                Platform.FileError(&File->Handle, "HHA file is of a later version");
            }
            if (PlatformNoFileErrors(&File->Handle)) {
                Assets->TagCount += (File->Header.TagCount - 1);
                Assets->AssetCount += (File->Header.AssetCount - 1);
            } else {
                InvalidCodePath
            }
        }
        Platform.GetAllFilesOfTypeEnd(&FileGroup);
    }

    Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);
    Assets->Tags = PushArray(Arena, Assets->TagCount, hha_tag);

    ZeroStruct(Assets->Tags[0]);

    // Tags
    for (uint32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex) {
        asset_file* File = Assets->Files + FileIndex;
        if (PlatformNoFileErrors(&File->Handle)) {
            uint32 TagArraySize = sizeof(hha_tag) * (File->Header.TagCount - 1);
            Platform.ReadDataFromFile(
                &File->Handle,
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
            if (PlatformNoFileErrors(&File->Handle)) {
                for (uint32 SourceIndex = 0; SourceIndex < File->Header.AssetTypeCount; ++SourceIndex) {
                    hha_asset_type* SourceType = File->AssetTypeArray + SourceIndex;
                    if (SourceType->TypeID == DestTypeID) {
                        uint32 AssetCountForType = SourceType->OnePastLastAssetIndex - SourceType->FirstAssetIndex;
                        temporary_memory TempMem = BeginTemporaryMemory(Arena);
                        hha_asset* HHAAssetArray = PushArray(TempMem.Arena, AssetCountForType, hha_asset);
                        Platform.ReadDataFromFile(
                            &File->Handle,
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

    return Assets;
}

internal void
BeginAssetLock(game_assets* Assets) {
    for (;;) {
        if (AtomicCompareExchangeUint32((volatile uint32*)&Assets->OperationLock, 1, 0) == 0) {
            break;
        }
    }
}

internal void EndAssetLock(game_assets* Assets) {
    CompletePreviousWritesBeforeFutureWrites;
    Assets->OperationLock = 0;
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

internal asset_memory_header* GetAsset(game_assets* Assets, uint32 ID, uint32 GenerationID) {

    Assert(ID <= Assets->AssetCount);
    asset* Asset = Assets->Assets + ID;

    asset_memory_header* Result = 0;

    BeginAssetLock(Assets);

    if (Asset->State == AssetState_Loaded) {

        Result = Asset->Header;

        RemoveAssetHeaderFromList(Result);
        InsertAssetheaderAtFront(Assets, Result);

        if (Asset->Header->GenerationID < GenerationID) {
            Asset->Header->GenerationID = GenerationID;
        }

        CompletePreviousWritesBeforeFutureWrites;
    }

    EndAssetLock(Assets);

    return Result;
}

internal inline loaded_bitmap* GetBitmap(game_assets* Assets, bitmap_id ID, uint32 GenerationID) {
    asset_memory_header* Header = GetAsset(Assets, ID.Value, GenerationID);
    loaded_bitmap* Result = Header ? &Header->Bitmap : 0;
    return Result;
}

internal inline loaded_sound* GetSound(game_assets* Assets, sound_id ID, uint32 GenerationID) {
    asset_memory_header* Header = GetAsset(Assets, ID.Value, GenerationID);
    loaded_sound* Result = Header ? &Header->Sound : 0;
    return Result;
}

internal inline hha_sound* GetSoundInfo(game_assets* Assets, sound_id ID) {
    Assert(ID.Value <= Assets->AssetCount);
    hha_sound* Result = &Assets->Assets[ID.Value].HHA.Sound;
    return Result;
}

internal inline hha_bitmap* GetBitmapInfo(game_assets* Assets, bitmap_id ID) {
    Assert(ID.Value <= Assets->AssetCount);
    hha_bitmap* Result = &Assets->Assets[ID.Value].HHA.Bitmap;
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

internal bool32 MergeIfPossible(game_assets* Assets, asset_memory_block* First, asset_memory_block* Second) {
    bool32 Result = false;
    if (First != &Assets->MemorySentinel && Second != &Assets->MemorySentinel) {
        if (!(First->Flags & AssetMemory_Used) && !(Second->Flags & AssetMemory_Used)) {
            uint8* ExpectedSecond = (uint8*)First + sizeof(asset_memory_block) + First->Size;
            if ((uint8*)Second == ExpectedSecond) {
                Second->Next->Prev = Second->Prev;
                Second->Prev->Next = Second->Next;
                First->Size += sizeof(asset_memory_block) + Second->Size;
                Result = true;
            }
        }
    }
    return Result;
}

internal bool32 GenerationHasCompleted(game_assets* Assets, uint32 CheckID) {
    bool32 Result = true;
    for (uint32 Index = 0; Index < ArrayCount(Assets->InFlightGenerations); ++Index) {
        if (Assets->InFlightGenerations[Index] == CheckID) {
            Result = false;
            break;
        }
    }
    return Result;
}

internal asset_memory_header* AcquireAssetMemory(game_assets* Assets, uint32 Size, uint32 AssetIndex) {
    asset_memory_header* Result = 0;

    BeginAssetLock(Assets);

    asset_memory_block* Block = FindBlockForSize(Assets, Size);
    for (;;) {
        if (Block && Size <= Block->Size) {
            Block->Flags |= AssetMemory_Used;

            Result = (asset_memory_header*)(Block + 1);

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
                if (Asset->State >= AssetState_Loaded && GenerationHasCompleted(Assets, Asset->Header->GenerationID)) {

                    Assert(Asset->State == AssetState_Loaded);

                    RemoveAssetHeaderFromList(Header);

                    Block = (asset_memory_block*)Asset->Header - 1;
                    Block->Flags &= ~AssetMemory_Used;
                    if (MergeIfPossible(Assets, Block->Prev, Block)) {
                        Block = Block->Prev;
                    }
                    MergeIfPossible(Assets, Block, Block->Next);

                    Asset->State = AssetState_Unloaded;
                    Asset->Header = 0;
                    break;
                }
            }
        }
    }

    if (Result) {
        Result->AssetIndex = AssetIndex;
        Result->TotalSize = Size;
        InsertAssetheaderAtFront(Assets, Result);
    }

    EndAssetLock(Assets);

    return Result;
}

internal uint32
BeginGeneration(game_assets* Assets) {
    BeginAssetLock(Assets);
    Assert(Assets->InFlightGenerationCount < ArrayCount(Assets->InFlightGenerations));
    uint32 Result = Assets->NextGenerationID++;
    Assets->InFlightGenerations[Assets->InFlightGenerationCount++] = Result;
    EndAssetLock(Assets);
    return Result;
}

internal void
EndGeneration(game_assets* Assets, uint32 GenerationID) {
    BeginAssetLock(Assets);
    for (uint32 Index = 0; Index < ArrayCount(Assets->InFlightGenerations); ++Index) {
        if (Assets->InFlightGenerations[Index] == GenerationID) {
            Assets->InFlightGenerations[Index] = Assets->InFlightGenerations[--Assets->InFlightGenerationCount];
            break;
        }
    }
    EndAssetLock(Assets);
}

#endif
