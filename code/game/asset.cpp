#if !defined(HANDMADE_ASSET)
#define HANDMADE_ASSET

#include "../types.h"
#include "math.cpp"
#include "bmp.cpp"
#include "memory.cpp"

enum asset_tag_id {
    Tag_Smoothness,
    Tag_Flatness,
    Tag_Count
};

enum asset_type_id {
    Asset_Backdrop,
    Asset_Shadow,
    Asset_Tree,
    Asset_Sword,
    Asset_Stairwell,
    Asset_Rock,
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
    uint32 Count;
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
    real32 WidthOverHeight;
    int32 Width;
    int32 Height;
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
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
    debug_platform_read_entire_file* ReadEntireFile;

    uint32 BitmapCount;
    asset_slot* Bitmaps;

    uint32 SoundCount;
    asset_slot* Sounds;

    asset_type Assets[Asset_Count];

    loaded_bitmap Grass[2];
    loaded_bitmap Ground[4];
    loaded_bitmap Tuft[3];

    hero_bitmaps HeroBitmaps[4];
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

internal game_assets* AllocateGameAssets(memory_arena* Arena, memory_index Size, transient_state* TranState, debug_platform_read_entire_file* ReadEntireFile) {
    game_assets* Assets = PushStruct(Arena, game_assets);
    SubArena(&Assets->Arena, Arena, Size);
    Assets->ReadEntireFile = ReadEntireFile;
    Assets->TranState = TranState;

    Assets->BitmapCount = Asset_Count;
    Assets->Bitmaps = PushArray(&Assets->Arena, Assets->BitmapCount, asset_slot);

    Assets->SoundCount = 1;
    Assets->Sounds = PushArray(&Assets->Arena, Assets->SoundCount, asset_slot);

    Assets->Grass[0] =
        DEBUGLoadBMP(ReadEntireFile, "test2/grass00.bmp");
    Assets->Grass[1] =
        DEBUGLoadBMP(ReadEntireFile, "test2/grass01.bmp");
    Assets->Tuft[0] =
        DEBUGLoadBMP(ReadEntireFile, "test2/tuft00.bmp");
    Assets->Tuft[1] =
        DEBUGLoadBMP(ReadEntireFile, "test2/tuft01.bmp");
    Assets->Tuft[2] =
        DEBUGLoadBMP(ReadEntireFile, "test2/tuft02.bmp");
    Assets->Ground[0] =
        DEBUGLoadBMP(ReadEntireFile, "test2/ground00.bmp");
    Assets->Ground[1] =
        DEBUGLoadBMP(ReadEntireFile, "test2/ground01.bmp");
    Assets->Ground[2] =
        DEBUGLoadBMP(ReadEntireFile, "test2/ground02.bmp");
    Assets->Ground[3] =
        DEBUGLoadBMP(ReadEntireFile, "test2/ground03.bmp");

    hero_bitmaps* Bitmap;

    Bitmap = &Assets->HeroBitmaps[0];
    Bitmap->Head =
        DEBUGLoadBMP(ReadEntireFile, "test/test_hero_right_head.bmp");
    Bitmap->Cape =
        DEBUGLoadBMP(ReadEntireFile, "test/test_hero_right_cape.bmp");
    Bitmap->Torso =
        DEBUGLoadBMP(ReadEntireFile, "test/test_hero_right_torso.bmp");
    SetTopDownAlign(Bitmap, V2(72, 182));

    ++Bitmap;
    Bitmap->Head =
        DEBUGLoadBMP(ReadEntireFile, "test/test_hero_back_head.bmp");
    Bitmap->Cape =
        DEBUGLoadBMP(ReadEntireFile, "test/test_hero_back_cape.bmp");
    Bitmap->Torso =
        DEBUGLoadBMP(ReadEntireFile, "test/test_hero_back_torso.bmp");
    SetTopDownAlign(Bitmap, V2(72, 182));

    ++Bitmap;
    Bitmap->Head =
        DEBUGLoadBMP(ReadEntireFile, "test/test_hero_left_head.bmp");
    Bitmap->Cape =
        DEBUGLoadBMP(ReadEntireFile, "test/test_hero_left_cape.bmp");
    Bitmap->Torso =
        DEBUGLoadBMP(ReadEntireFile, "test/test_hero_left_torso.bmp");
    SetTopDownAlign(Bitmap, V2(72, 182));

    ++Bitmap;
    Bitmap->Head =
        DEBUGLoadBMP(ReadEntireFile, "test/test_hero_front_head.bmp");
    Bitmap->Cape =
        DEBUGLoadBMP(ReadEntireFile, "test/test_hero_front_cape.bmp");
    Bitmap->Torso =
        DEBUGLoadBMP(ReadEntireFile, "test/test_hero_front_torso.bmp");
    SetTopDownAlign(Bitmap, V2(72, 182));

    return Assets;
}

internal inline loaded_bitmap* GetBitmap(game_assets* Assets, bitmap_id ID) {
    loaded_bitmap* Result = Assets->Bitmaps[ID.Value].Bitmap;
    return Result;
}

internal bitmap_id GetFirstBitmapID(game_assets* Assets, asset_type_id TypeID) {
    bitmap_id Result = { (uint32)TypeID };
    return Result;
}

#endif