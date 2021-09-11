#if !defined(HANDMADE_ASSET)
#define HANDMADE_ASSET

#include "../types.h"
#include "math.cpp"
#include "bmp.cpp"
#include "memory.cpp"
#include "random.cpp"

enum asset_tag_id {
    Tag_Smoothness,
    Tag_Flatness,
    Tag_Count
};

enum asset_type_id {
    Asset_None,
    Asset_Shadow,
    Asset_Tree,
    Asset_Sword,
    Asset_Stairwell,
    Asset_Rock,
    Asset_Grass,
    Asset_Tuft,
    Asset_Stone,
    Asset_Count
};

struct asset_tag {
    uint32 ID;
    real32 Value;
};

struct asset {
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
    uint32 SlotID;
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

struct asset_slot {
    asset_state State;
    loaded_bitmap* Bitmap;
};


struct asset_bitmap_info {
    v2 AlignPercentage;
    char* Filename;
};

struct asset_group {
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
};

struct hero_bitmaps {
    loaded_bitmap Head;
    loaded_bitmap Cape;
    loaded_bitmap Torso;
};


struct game_assets {
    struct transient_state* TranState;
    memory_arena Arena;

    uint32 BitmapCount;
    uint32 DEBUGUsedBitmapCount;
    asset_bitmap_info* BitmapInfos;
    asset_slot* Bitmaps;

    uint32 SoundCount;
    asset_slot* Sounds;

    uint32 TagCount;
    asset_tag* Tags;

    uint32 AssetCount;
    uint32 DEBUGUsedAssetCount;
    asset* Assets;

    asset_type AssetTypes[Asset_Count];

    hero_bitmaps HeroBitmaps[4];

    asset_type* DEBUGAssetType;
};

struct bitmap_id {
    uint32 Value;
};

internal void SetTopDownAlign(loaded_bitmap* Bitmap, v2 Align) {
    Align = TopDownAlign(Bitmap, Align);
    Bitmap->AlignPercentage = Align;
}

internal void SetTopDownAlign(hero_bitmaps* Bitmap, v2 Align) {
    SetTopDownAlign(&Bitmap->Head, Align);
    SetTopDownAlign(&Bitmap->Cape, Align);
    SetTopDownAlign(&Bitmap->Torso, Align);
}

internal bitmap_id DEBUGAddBitmapInfo(game_assets* Assets, char* Filename, v2 AlignPercentage) {
    Assert(Assets->DEBUGUsedBitmapCount < Assets->BitmapCount);
    bitmap_id ID = { Assets->DEBUGUsedBitmapCount++ };
    asset_bitmap_info* Info = Assets->BitmapInfos + ID.Value;
    Info->AlignPercentage = AlignPercentage;
    Info->Filename = Filename;
    return ID;
}

internal void BeginAssetType(game_assets* Assets, asset_type_id Type) {
    Assert(Assets->DEBUGAssetType == 0);
    Assets->DEBUGAssetType = Assets->AssetTypes + Type;
    Assets->DEBUGAssetType->FirstAssetIndex = Assets->DEBUGUsedAssetCount;
    Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

internal void
AddBitmapAsset(game_assets* Assets, char* Filename, v2 AlignPercentage = V2(0.5f, 0.5f)) {
    Assert(Assets->DEBUGAssetType != 0);
    asset* Asset = Assets->Assets + Assets->DEBUGAssetType->OnePastLastAssetIndex++;
    Asset->FirstTagIndex = 0;
    Asset->OnePastLastTagIndex = 0;
    Asset->SlotID = DEBUGAddBitmapInfo(Assets, Filename, AlignPercentage).Value;
}

internal void EndAssetType(game_assets* Assets) {
    Assert(Assets->DEBUGAssetType != 0);
    Assets->DEBUGUsedAssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
    Assets->DEBUGAssetType = 0;
}

internal game_assets* AllocateGameAssets(memory_arena* Arena, memory_index Size, transient_state* TranState) {
    game_assets* Assets = PushStruct(Arena, game_assets);
    SubArena(&Assets->Arena, Arena, Size);
    Assets->TranState = TranState;

    Assets->BitmapCount = 256 * Asset_Count;
    Assets->DEBUGUsedBitmapCount = 1;
    Assets->Bitmaps = PushArray(Arena, Assets->BitmapCount, asset_slot);
    Assets->BitmapInfos = PushArray(Arena, Assets->BitmapCount, asset_bitmap_info);

    Assets->SoundCount = 1;
    Assets->Sounds = PushArray(Arena, Assets->SoundCount, asset_slot);

    Assets->TagCount = 0;
    Assets->Tags = 0;

    Assets->AssetCount = Assets->BitmapCount + Assets->SoundCount;
    Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);

    Assets->DEBUGUsedBitmapCount = 1;
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

    hero_bitmaps* Bitmap;

    Bitmap = &Assets->HeroBitmaps[0];
    Bitmap->Head = DEBUGLoadBMP("test/test_hero_right_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP("test/test_hero_right_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP("test/test_hero_right_torso.bmp");
    SetTopDownAlign(Bitmap, V2(72, 182));

    ++Bitmap;
    Bitmap->Head = DEBUGLoadBMP("test/test_hero_back_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP("test/test_hero_back_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP("test/test_hero_back_torso.bmp");
    SetTopDownAlign(Bitmap, V2(72, 182));

    ++Bitmap;
    Bitmap->Head = DEBUGLoadBMP("test/test_hero_left_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP("test/test_hero_left_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP("test/test_hero_left_torso.bmp");
    SetTopDownAlign(Bitmap, V2(72, 182));

    ++Bitmap;
    Bitmap->Head = DEBUGLoadBMP("test/test_hero_front_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP("test/test_hero_front_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP("test/test_hero_front_torso.bmp");
    SetTopDownAlign(Bitmap, V2(72, 182));

    return Assets;
}

internal inline loaded_bitmap* GetBitmap(game_assets* Assets, bitmap_id ID) {
    loaded_bitmap* Result = Assets->Bitmaps[ID.Value].Bitmap;
    return Result;
}

internal bitmap_id
RandomAssetFrom(game_assets* Assets, asset_type_id TypeID, random_series* Series) {
    bitmap_id Result = {};
    asset_type* Type = Assets->AssetTypes + TypeID;
    if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex) {
        uint32 Count = Type->OnePastLastAssetIndex - Type->FirstAssetIndex;
        uint32 Choice = RandomChoice(Series, Count);
        asset* Asset = Assets->Assets + Type->FirstAssetIndex + Choice;
        Result.Value = Asset->SlotID;
    }
    return Result;
}

internal bitmap_id GetFirstBitmapID(game_assets* Assets, asset_type_id TypeID) {
    bitmap_id Result = {};
    asset_type* Type = Assets->AssetTypes + TypeID;
    if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex) {
        asset* Asset = Assets->Assets + Type->FirstAssetIndex;
        Result.Value = Asset->SlotID;
    }
    return Result;
}

#endif
