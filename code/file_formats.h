#if !defined(HANDMADE_FILE_FORMATS_H)
#define HANDMADE_FILE_FORMATS_H

#include "types.h"

enum asset_type_id {
    Asset_None,
    Asset_Shadow,
    Asset_Tree,
    Asset_Sword,
    //Asset_Stairwell,
    Asset_Rock,
    Asset_Grass,
    Asset_Tuft,
    Asset_Stone,
    Asset_Head,
    Asset_Cape,
    Asset_Torso,

    Asset_Font,
    Asset_FontGlyph,

    // Sounds
    Asset_Bloop,
    Asset_Crack,
    Asset_Drop,
    Asset_Glide,
    Asset_Music,
    Asset_Puhp,

    Asset_Count
};

enum asset_tag_id {
    Tag_Smoothness,
    Tag_Flatness,
    Tag_FacingDirection, // Angle in radians off of due right
    Tag_UnicodeCodepoint,

    Tag_Count
};

#pragma pack(push, 1)

#define HHA_CODE(a,b,c,d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))
#define HHA_MAGIC_VALUE HHA_CODE('h','h','a','f')
#define HHA_VERSION 0

struct sound_id {
    uint32 Value;
};

struct bitmap_id {
    uint32 Value;
};

struct font_id {
    uint32 Value;
};

struct hha_header {
    uint32 MagicValue;
    uint32 Version;
    uint32 TagCount;
    uint32 AssetCount;
    uint32 AssetTypeCount;
    uint64 Tags;
    uint64 Assets;
    uint64 AssetTypes;
};

struct hha_tag {
    uint32 ID;
    real32 Value;
};

struct hha_asset_type {
    uint32 TypeID;
    uint32 FirstAssetIndex;
    uint32 OnePastLastAssetIndex;
};

enum hha_sound_chain {
    HHASoundChain_None,
    HHASoundChain_Loop,
    HHASoundChain_Advance,
};

struct hha_bitmap {
    uint32 Dim[2];
    real32 AlignPercentage[2];
};

struct hha_sound {
    uint32 SampleCount;
    uint32 ChannelCount;
    uint32 Chain;
};

struct hha_font {
    uint32 CodepointCount;
    real32 LineAdvance;
};

struct hha_asset {
    uint64 DataOffset;
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
    union {
        hha_bitmap Bitmap;
        hha_sound Sound;
        hha_font Font;
    };
};

#pragma pack(pop)

#endif
