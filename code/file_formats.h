#if !defined(HANDMADE_FILE_FORMATS_H)
#define HANDMADE_FILE_FORMATS_H

#include "types.h"

#pragma pack(push, 1)

#define HHA_CODE(a,b,c,d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))
#define HHA_MAGIC_VALUE HHA_CODE('h','h','a','f')
#define HHA_VERSION 0

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

struct hha_bitmap {
    uint32 Dim[2];
    real32 AlignPercentage[2];
};

struct hha_sound {
    uint32 FirstSampleIndex;
    uint32 SampleCount;
    uint32 NextIDToPlay;
};

struct hha_asset {
    uint64 DataOffset;
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
    union {
        hha_bitmap Bitmap;
        hha_sound Sound;
    };
};

#pragma pack(pop)

#endif
