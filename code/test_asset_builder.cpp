#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "util.h"
#include "game/asset_type_id.h"
#include "file_formats.h"

FILE* Out = 0;

struct bitmap_id {
    uint32 Value;
};

struct sound_id {
    uint32 Value;
};

struct asset_bitmap_info {
    char* Filename;
    real32 AlignPercentage[2];
};

struct asset_sound_info {
    char* Filename;
    uint32 FirstSampleIndex;
    uint32 SampleCount;
    sound_id NextIDToPlay;
};

struct asset {
    uint64 DataOffset;
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
    union {
        asset_bitmap_info Bitmap;
        asset_sound_info Sound;
    };
};

#define VERY_LARGE_NUMBER 4096

struct game_assets {
    uint32 TagCount;
    hha_tag Tags[VERY_LARGE_NUMBER];

    uint32 AssetCount;
    asset Assets[VERY_LARGE_NUMBER];

    uint32 AssetTypeCount;
    hha_asset_type AssetTypes[Asset_Count];

    hha_asset_type* DEBUGAssetType;
    asset* DEBUGAsset;
};

internal void BeginAssetType(game_assets* Assets, asset_type_id Type) {
    Assert(Assets->DEBUGAssetType == 0);
    Assets->DEBUGAssetType = Assets->AssetTypes + Type;
    Assets->DEBUGAssetType->TypeID = Type;
    Assets->DEBUGAssetType->FirstAssetIndex = Assets->AssetCount;
    Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

internal bitmap_id
AddBitmapAsset(game_assets* Assets, char* Filename, real32 AlignPercentageX = 0.5f, real32 AlignPercentageY = 0.5f) {
    Assert(Assets->DEBUGAssetType != 0);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets->Assets));
    bitmap_id Result = { Assets->DEBUGAssetType->OnePastLastAssetIndex++ };
    asset* Asset = Assets->Assets + Result.Value;
    Asset->FirstTagIndex = Assets->TagCount;
    Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
    Asset->Bitmap.Filename = Filename;
    Asset->Bitmap.AlignPercentage[0] = AlignPercentageX;
    Asset->Bitmap.AlignPercentage[1] = AlignPercentageY;
    Assets->DEBUGAsset = Asset;
    return Result;
}

internal sound_id
AddSoundAsset(game_assets* Assets, char* Filename, uint32 FirstSampleIndex = 0, uint32 SampleCount = 0) {
    Assert(Assets->DEBUGAssetType != 0);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets->Assets));
    sound_id Result = { Assets->DEBUGAssetType->OnePastLastAssetIndex++ };
    asset* Asset = Assets->Assets + Result.Value;
    Asset->FirstTagIndex = Assets->TagCount;
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
    hha_tag* Tag = Assets->Tags + Assets->TagCount++;
    Tag->ID = TagID;
    Tag->Value = Value;
}

internal void EndAssetType(game_assets* Assets) {
    Assert(Assets->DEBUGAssetType != 0);
    Assets->AssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
    Assets->DEBUGAssetType = 0;
    Assets->DEBUGAsset = 0;
}

int main(int ArgCount, char** Args) {
    game_assets Assets_;
    game_assets* Assets = &Assets_;

    Assets->TagCount = 1;
    Assets->AssetCount = 1;
    Assets->DEBUGAssetType = 0;
    Assets->DEBUGAsset = 0;
#if 1
    BeginAssetType(Assets, Asset_Shadow);
    AddBitmapAsset(Assets, "test/test_hero_shadow.bmp[2]", 0.5f, 0.15668f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Tree);
    AddBitmapAsset(Assets, "test2/tree00.bmp[2]", 0.494f, 0.295f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Sword);
    AddBitmapAsset(Assets, "test2/rock03.bmp[2]", 0.5f, 0.656f);
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

    real32 HeroAlign[2] = { 0.5f, 0.1567 };

    BeginAssetType(Assets, Asset_Head);
    AddBitmapAsset(Assets, "test/test_hero_right_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/test_hero_back_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/test_hero_left_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/test_hero_front_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Cape);
    AddBitmapAsset(Assets, "test/test_hero_right_cape.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/test_hero_back_cape.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/test_hero_left_cape.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/test_hero_front_cape.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Torso);
    AddBitmapAsset(Assets, "test/test_hero_right_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/test_hero_back_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/test_hero_left_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/test_hero_front_torso.bmp", HeroAlign[0], HeroAlign[1]);
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
        if (LastMusic.Value) {
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
    Out = fopen("test.hha", "wb");
    if (Out) {
        hha_header Header = {};
        Header.MagicValue = HHA_MAGIC_VALUE;
        Header.Version = HHA_VERSION;
        Header.TagCount = Assets->TagCount;
        Header.AssetTypeCount = Asset_Count;
        Header.AssetCount = Assets->AssetCount;

        uint32 TagsSize = sizeof(hha_tag) * Header.TagCount;
        uint32 AssetTypesSize = sizeof(hha_asset_type) * Header.AssetTypeCount;
        uint32 AssetsSize = sizeof(hha_asset) * Header.AssetCount;

        Header.Tags = sizeof(Header);
        Header.AssetTypes = Header.Tags + TagsSize;
        Header.Assets = Header.AssetTypes + AssetTypesSize;

        fwrite(&Header, sizeof(Header), 1, Out);
        fwrite(Assets->Tags, TagsSize, 1, Out);
        fwrite(Assets->AssetTypes, AssetTypesSize, 1, Out);
        // fwrite(AssetArray, AssetsSize, 1, Out);

        fclose(Out);
    } else {
        printf("ERROR: Couldn't open file\n");
    }
}
