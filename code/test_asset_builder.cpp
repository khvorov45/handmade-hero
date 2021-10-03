#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "types.h"
#include "util.h"
#include "intrinsics.h"
#include "file_formats.h"
#include "game/math.cpp"

#define USE_FONTS_FROM_WINDOWS 1

#if USE_FONTS_FROM_WINDOWS
#include "windows.h"
#else
#define STB_TRUETYPE_IMPLEMENTATION 1
#include "stb_truetype.h"
#endif

enum asset_type {
    AssetType_Sound,
    AssetType_Bitmap,
    AssetType_Font,
    AssetType_FontGlyph,
};

struct loaded_font {
    HFONT Win32Handle;
    TEXTMETRIC TextMetric;
    uint32 CodepointCount;

    bitmap_id* BitmapIDs;
    real32* HorizontalAdvance;
};

struct asset_source_font {
    loaded_font* Font;
};

struct asset_source_font_glyph {
    loaded_font* Font;
    uint32 Codepoint;
};

struct asset_source_sound {
    char* Filename;
    uint32 FirstSampleIndex;
};

struct asset_source_bitmap {
    char* Filename;
    uint32 FirstSampleIndex;
};

struct asset_source {
    asset_type Type;
    union {
        asset_source_bitmap Bitmap;
        asset_source_sound Sound;
        asset_source_font Font;
        asset_source_font_glyph Glyph;
    };
};

#define VERY_LARGE_NUMBER 4096

struct game_assets {
    uint32 TagCount;
    hha_tag Tags[VERY_LARGE_NUMBER];

    uint32 AssetCount;
    asset_source AssetSources[VERY_LARGE_NUMBER];
    hha_asset Assets[VERY_LARGE_NUMBER];

    uint32 AssetTypeCount;
    hha_asset_type AssetTypes[Asset_Count];

    hha_asset_type* DEBUGAssetType;
    uint32 AssetIndex;
};

internal void BeginAssetType(game_assets* Assets, asset_type_id Type) {
    Assert(Assets->DEBUGAssetType == 0);
    Assets->DEBUGAssetType = Assets->AssetTypes + Type;
    Assets->DEBUGAssetType->TypeID = Type;
    Assets->DEBUGAssetType->FirstAssetIndex = Assets->AssetCount;
    Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

struct added_asset {
    uint32 ID;
    hha_asset* HHA;
    asset_source* Source;
};

internal added_asset AddAsset(game_assets* Assets) {
    Assert(Assets->DEBUGAssetType != 0);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets->Assets));
    uint32 Index = Assets->DEBUGAssetType->OnePastLastAssetIndex++;
    hha_asset* HHA = Assets->Assets + Index;
    HHA->FirstTagIndex = Assets->TagCount;
    HHA->OnePastLastTagIndex = HHA->FirstTagIndex;

    Assets->AssetIndex = Index;

    asset_source* Source = Assets->AssetSources + Index;

    added_asset Result = {};
    Result.ID = Index;
    Result.HHA = HHA;
    Result.Source = Source;
    return Result;
}

internal bitmap_id
AddBitmapAsset(game_assets* Assets, char* Filename, real32 AlignPercentageX = 0.5f, real32 AlignPercentageY = 0.5f) {
    added_asset Asset = AddAsset(Assets);
    Asset.HHA->Bitmap.AlignPercentage[0] = AlignPercentageX;
    Asset.HHA->Bitmap.AlignPercentage[1] = AlignPercentageY;
    Asset.Source->Type = AssetType_Bitmap;
    Asset.Source->Bitmap.Filename = Filename;
    bitmap_id Result = { Asset.ID };
    return Result;
}

internal font_id
AddFontAsset(game_assets* Assets, loaded_font* Font) {
    added_asset Asset = AddAsset(Assets);
    Asset.HHA->Font.CodepointCount = Font->CodepointCount;
    Asset.HHA->Font.ExternalLeading = (real32)Font->TextMetric.tmExternalLeading;
    Asset.HHA->Font.AscenderHeight = (real32)Font->TextMetric.tmAscent;
    Asset.HHA->Font.DescenderHeight = (real32)Font->TextMetric.tmDescent;
    Asset.Source->Type = AssetType_Font;
    Asset.Source->Font.Font = Font;
    font_id Result = { Asset.ID };
    return Result;
}

bitmap_id
AddCharacterAsset(
    game_assets* Assets, loaded_font* Font,
    uint32 Codepoint
) {
    added_asset Asset = AddAsset(Assets);
    Asset.HHA->Bitmap.AlignPercentage[0] = 0.0f;
    Asset.HHA->Bitmap.AlignPercentage[1] = 0.0f;
    Asset.Source->Type = AssetType_FontGlyph;
    Asset.Source->Glyph.Font = Font;
    Asset.Source->Glyph.Codepoint = Codepoint;
    bitmap_id Result = { Asset.ID };
    return Result;
}

internal sound_id
AddSoundAsset(game_assets* Assets, char* Filename, uint32 FirstSampleIndex = 0, uint32 SampleCount = 0) {
    added_asset Asset = AddAsset(Assets);
    Asset.HHA->Sound.SampleCount = SampleCount;
    Asset.HHA->Sound.Chain = HHASoundChain_None;
    Asset.Source->Type = AssetType_Sound;
    Asset.Source->Sound.Filename = Filename;
    Asset.Source->Sound.FirstSampleIndex = FirstSampleIndex;
    sound_id Result = { Asset.ID };
    return Result;
}

internal void AddTag(game_assets* Assets, asset_tag_id TagID, real32 Value) {
    Assert(Assets->AssetIndex);
    hha_asset* HHA = Assets->Assets + Assets->AssetIndex;
    ++HHA->OnePastLastTagIndex;
    hha_tag* Tag = Assets->Tags + Assets->TagCount++;
    Tag->ID = TagID;
    Tag->Value = Value;
}

internal void EndAssetType(game_assets* Assets) {
    Assert(Assets->DEBUGAssetType != 0);
    Assets->AssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
    Assets->DEBUGAssetType = 0;
    Assets->AssetIndex = 0;
}

struct entire_file {
    uint32 ContentsSize;
    void* Contents;
};

entire_file
ReadEntireFile(char* Filename) {
    entire_file Result = {};
    FILE* In = fopen(Filename, "rb");
    if (In) {
        fseek(In, 0, SEEK_END);
        Result.ContentsSize = ftell(In);
        fseek(In, 0, SEEK_SET);
        Result.Contents = malloc(Result.ContentsSize);
        fread(Result.Contents, Result.ContentsSize, 1, In);
        fclose(In);
    } else {
        printf("ERROR: Cannot open %s\n", Filename);
    }
    return Result;
}

struct loaded_bitmap {
    int32 Width;
    int32 Height;
    int32 Pitch;
    void* Memory;
    void* Free;
};

#pragma pack(push, 1)
struct bitmap_header {
    uint16 FileType;
    uint32 FileSize;
    uint16 Reserved1;
    uint16 Reserved2;
    uint32 BitmapOffset;
    uint32 Size;
    int32 Width;
    int32 Height;
    uint16 Planes;
    uint16 BitsPerPixel;
    uint32 Compression;
    uint32 SizeOfBitmap;
    int32 HorzResolution;
    int32 VertResolution;
    uint32 ColorsUsed;
    uint32 ColorsImportant;
    uint32 RedMask;
    uint32 GreenMask;
    uint32 BlueMask;
};
#pragma pack(pop)

#define BITMAP_BYTES_PER_PIXEL 4

internal loaded_bitmap
LoadBMP(char* Filename) {
    entire_file ReadResult = ReadEntireFile(Filename);
    loaded_bitmap Result = {};
    if (ReadResult.ContentsSize != 0) {
        Result.Free = ReadResult.Contents;
        bitmap_header* Header = (bitmap_header*)(ReadResult.Contents);

        Assert(Header->Compression == 3);

        Result.Height = Header->Height;
        Assert(Result.Height >= 0);
        Result.Width = Header->Width;
        Result.Pitch = Result.Width * BITMAP_BYTES_PER_PIXEL;

        uint32 RedMask = Header->RedMask;
        uint32 GreenMask = Header->GreenMask;
        uint32 BlueMask = Header->BlueMask;
        uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

        bit_scan_result RedResult = FindLeastSignificantSetBit(RedMask);
        bit_scan_result GreenResult = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result BlueResult = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result AlphaResult = FindLeastSignificantSetBit(AlphaMask);

        Assert(RedResult.Found);
        Assert(GreenResult.Found);
        Assert(BlueResult.Found);
        Assert(AlphaResult.Found);

        uint32 RedShift = RedResult.Index;
        uint32 GreenShift = GreenResult.Index;
        uint32 BlueShift = BlueResult.Index;
        uint32 AlphaShift = AlphaResult.Index;

        uint32* Pixels = (uint32*)((uint8*)ReadResult.Contents + Header->BitmapOffset);
        uint32* Pixel = Pixels;
        for (int32 Y = 0; Y < Header->Height; ++Y) {
            for (int32 X = 0; X < Header->Width; ++X) {
                v4 Texel = V4(
                    (real32)((*Pixel >> RedShift) & 0xFF),
                    (real32)((*Pixel >> GreenShift) & 0xFF),
                    (real32)((*Pixel >> BlueShift) & 0xFF),
                    (real32)((*Pixel >> AlphaShift) & 0xFF)
                );

                Texel = SRGB255ToLinear1(Texel);
                Texel.rgb *= Texel.a;
                Texel = Linear1ToSRGB255(Texel);

                *Pixel++ =
                    (RoundReal32ToUint32(Texel.a) << 24) |
                    (RoundReal32ToUint32(Texel.r) << 16) |
                    (RoundReal32ToUint32(Texel.g) << 8) |
                    (RoundReal32ToUint32(Texel.b));
            }
        }
        Result.Memory = (uint8*)Pixels;
    }
    return Result;
}

#define MAX_FONT_WIDTH 1024
#define MAX_FONT_HEIGHT 1024

global_variable VOID* GlobalFontBits;
global_variable HDC GlobalFontDeviceContext;

internal void
InitlializeFontDC() {
    GlobalFontDeviceContext = CreateCompatibleDC(GetDC(0));

    BITMAPINFO Info = {};
    Info.bmiHeader.biSize = sizeof(Info.bmiHeader);
    Info.bmiHeader.biWidth = MAX_FONT_WIDTH;
    Info.bmiHeader.biHeight = MAX_FONT_HEIGHT;
    Info.bmiHeader.biPlanes = 1;
    Info.bmiHeader.biBitCount = 32;
    Info.bmiHeader.biCompression = BI_RGB;
    Info.bmiHeader.biSizeImage = 0;
    Info.bmiHeader.biXPelsPerMeter = 0;
    Info.bmiHeader.biYPelsPerMeter = 0;
    Info.bmiHeader.biClrUsed = 0;
    Info.bmiHeader.biClrImportant = 0;
    HBITMAP Bitmap = CreateDIBSection(GlobalFontDeviceContext, &Info, DIB_RGB_COLORS, &GlobalFontBits, 0, 0);
    SelectObject(GlobalFontDeviceContext, Bitmap);

    SetBkColor(GlobalFontDeviceContext, RGB(0, 0, 0));
}

internal loaded_font LoadFont(char* FontFile, char* FontName, uint32 CodepointCount) {
    loaded_font Font = {};
    AddFontResourceExA(FontFile, FR_PRIVATE, 0);
    int32 Height = 128;
    Font.Win32Handle = CreateFontA(
        Height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        FontName
    );
    SelectObject(GlobalFontDeviceContext, Font.Win32Handle);
    GetTextMetrics(GlobalFontDeviceContext, &Font.TextMetric);
    Font.CodepointCount = CodepointCount;
    Font.BitmapIDs = (bitmap_id*)malloc(sizeof(bitmap_id) * CodepointCount);
    Font.HorizontalAdvance = (real32*)malloc(sizeof(real32) * CodepointCount * CodepointCount);

    ABC* Widths = (ABC*)malloc(sizeof(ABC) * CodepointCount);
    GetCharABCWidthsW(GlobalFontDeviceContext, 0, CodepointCount - 1, Widths);

    for (uint32 CodepointIndex = 0; CodepointIndex < Font.CodepointCount; ++CodepointIndex) {
        real32 TotalWidth = 0.0f;
        if (CodepointIndex != 0) {
            ABC* Width = Widths + CodepointIndex;
            TotalWidth = (real32)(Width->abcB + Width->abcA + Width->abcC);
        }
        for (uint32 OtherCodepointIndex = 0; OtherCodepointIndex < Font.CodepointCount; ++OtherCodepointIndex) {
            Font.HorizontalAdvance[CodepointIndex * Font.CodepointCount + OtherCodepointIndex] = TotalWidth;
        }
    }

    free(Widths);

    DWORD KerningPairCount = GetKerningPairsW(GlobalFontDeviceContext, 0, 0);
    KERNINGPAIR* KerningPairs = (KERNINGPAIR*)malloc(sizeof(KERNINGPAIR) * KerningPairCount);
    GetKerningPairsW(GlobalFontDeviceContext, KerningPairCount, KerningPairs);

    for (uint32 KerningPairIndex = 0; KerningPairIndex < KerningPairCount; ++KerningPairIndex) {
        KERNINGPAIR* Pair = KerningPairs + KerningPairIndex;
        if (Pair->wFirst < Font.CodepointCount && Pair->wSecond < Font.CodepointCount) {
            Font.HorizontalAdvance[Pair->wFirst * Font.CodepointCount + Pair->wSecond] += Pair->iKernAmount;
        }
    }

    free(KerningPairs);
    return Font;
}

internal void FreeFont(loaded_font* Font) {
    DeleteObject(Font->Win32Handle);
}

internal loaded_bitmap
LoadGlyphBitmap(loaded_font* Font, uint32 Codepoint, hha_asset* Asset) {
    loaded_bitmap Result = {};

#if USE_FONTS_FROM_WINDOWS
    memset(GlobalFontBits, 0, MAX_FONT_WIDTH * MAX_FONT_HEIGHT * sizeof(uint32));

    SelectObject(GlobalFontDeviceContext, Font->Win32Handle);

    ABC ThisABC;
    GetCharABCWidthsW(GlobalFontDeviceContext, Codepoint, Codepoint, &ThisABC);

    wchar_t CheesePoint = (wchar_t)Codepoint;

    SIZE Size;
    GetTextExtentPoint32W(GlobalFontDeviceContext, &CheesePoint, 1, &Size);

    int32 Width = Size.cx;
    int32 Height = Size.cy;

    SetTextColor(GlobalFontDeviceContext, RGB(255, 255, 255));

    int32 PrestepX = 128;
    TextOutW(GlobalFontDeviceContext, PrestepX, 0, &CheesePoint, 1);

    int32 MinX = 10000;
    int32 MinY = 10000;
    int32 MaxX = -10000;
    int32 MaxY = -10000;

    uint32* Row = (uint32*)GlobalFontBits;
    for (int32 Y = 0; Y < MAX_FONT_HEIGHT; ++Y) {
        uint32* Pixel = Row;
        for (int32 X = 0; X < MAX_FONT_WIDTH; ++X) {
            //COLORREF RefPixel = GetPixel(DeviceContext, X, Y);
            //Assert(RefPixel == *Pixel);
            if (*Pixel != 0) {
                if (MinX > X) { MinX = X; }
                if (MaxX < X) { MaxX = X; }
                if (MinY > Y) { MinY = Y; }
                if (MaxY < Y) { MaxY = Y; }
            }
            ++Pixel;
        }
        Row += MAX_FONT_WIDTH;
    }

    if (MinX <= MaxX && MinY <= MaxY) {

        Width = MaxX - MinX + 1;
        Height = MaxY - MinY + 1;

        Result.Height = Height + 2;
        Result.Width = Width + 2;
        Result.Pitch = Result.Width * BITMAP_BYTES_PER_PIXEL;
        Result.Memory = malloc(Result.Height * Result.Pitch);
        memset(Result.Memory, 0, Result.Height * Result.Pitch);
        Result.Free = Result.Memory;

#if 0
        {
            uint8* DestRow = (uint8*)Result.Memory; //+ (Height - 1) * Result.Pitch;
            for (int32 Y = 0; Y < Result.Height; ++Y) {
                uint32* Dest = (uint32*)DestRow;
                for (int32 X = 0; X < Result.Width; ++X) {
                    uint8 Gray = 0;
                    uint8 Alpha = 0xff;
                    *Dest++ = (Alpha << 24) | (Gray << 16) | (Gray << 8) | (Gray << 0);
                }
                DestRow += Result.Pitch;
            }
        }
#endif

        uint8* DestRow = (uint8*)Result.Memory + Result.Pitch + sizeof(uint32); //+ (Height - 1) * Result.Pitch;
        uint8* SourceRow = (uint8*)GlobalFontBits + MinY * MAX_FONT_WIDTH * sizeof(uint32) + MinX * sizeof(uint32);
        for (int32 Y = MinY; Y <= MaxY; ++Y) {
            uint32* Dest = (uint32*)DestRow;
            uint32* Source = (uint32*)SourceRow;
            for (int32 X = MinX; X <= MaxX; ++X) {
                uint8 Alpha = (uint8)(*Source++ & 0xFF);
                v4 Texel = V4(
                    (real32)(0xFF),
                    (real32)(0xFF),
                    (real32)(0xFF),
                    (real32)(Alpha)
                );

                Texel = SRGB255ToLinear1(Texel);
                Texel.rgb *= Texel.a;
                Texel = Linear1ToSRGB255(Texel);

                *Dest++ = (RoundReal32ToUint32(Texel.a) << 24) |
                    (RoundReal32ToUint32(Texel.r) << 16) |
                    (RoundReal32ToUint32(Texel.g) << 8) |
                    (RoundReal32ToUint32(Texel.b));
            }
            DestRow += Result.Pitch;
            SourceRow += MAX_FONT_WIDTH * sizeof(uint32);
        }

        Asset->Bitmap.AlignPercentage[0] = (1.0f - (MinX - PrestepX)) / ((real32)Result.Width);
        int32 YStartActual = MAX_FONT_HEIGHT - Font->TextMetric.tmHeight; // NOTE(sen) This would include the empty part of the character bitmap
        Assert(YStartActual <= MinY);
        int32 TrimmedDescent = Font->TextMetric.tmDescent - (MinY - YStartActual);
        Asset->Bitmap.AlignPercentage[1] = ((TrimmedDescent + 1.0f) / (real32)Result.Height);
    }

#else
    entire_file TTFFile = ReadEntireFile(FontFile);
    if (TTFFile.ContentsSize != 0) {
        stbtt_fontinfo Font;
        stbtt_InitFont(&Font, (uint8*)TTFFile.Contents, stbtt_GetFontOffsetForIndex((uint8*)TTFFile.Contents, 0));
        int32 Width, Height, XOffset, YOffset;
        uint8* MonoBitmap = stbtt_GetCodepointBitmap(&Font, 0, stbtt_ScaleForPixelHeight(&Font, 128.0f), Codepoint, &Width, &Height, &XOffset, &YOffset);

        Result.Height = Height;
        Result.Width = Width;
        Result.Pitch = Result.Width * BITMAP_BYTES_PER_PIXEL;
        Result.Memory = malloc(Height * Result.Pitch);
        Result.Free = Result.Memory;

        uint8* Source = MonoBitmap;
        uint8* DestRow = (uint8*)Result.Memory + (Height - 1) * Result.Pitch;
        for (int32 Y = 0; Y < Height; ++Y) {
            uint32* Dest = (uint32*)DestRow;
            for (int32 X = 0; X < Width; ++X) {
                uint8 Gray = *Source++;
                uint8 Alpha = 0xFF;
                *Dest++ = (Alpha << 24) | (Gray << 16) | (Gray << 8) | (Gray << 0);
            }
            DestRow -= Result.Pitch;
        }
        stbtt_FreeBitmap(MonoBitmap, 0);
        free(TTFFile.Contents);
    }
#endif
    return Result;
}

struct loaded_sound {
    uint32 SampleCount;
    uint32 ChannelCount;
    int16* Samples[2];
    void* Free;
};

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
LoadWAV(char* Filename, uint32 SectionFirstSampleIndex, uint32 SectionSampleCount) {
    loaded_sound Result = {};
    entire_file ReadResult = ReadEntireFile(Filename);
    if (ReadResult.ContentsSize != 0) {
        Result.Free = ReadResult.Contents;
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

internal void WriteHHA(game_assets* Assets, char* Filename) {
    FILE* Out = fopen(Filename, "wb");
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
        fseek(Out, AssetsSize, SEEK_CUR);
        for (uint32 AssetIndex = 1; AssetIndex < Header.AssetCount; ++AssetIndex) {

            asset_source* Source = Assets->AssetSources + AssetIndex;
            hha_asset* Dest = Assets->Assets + AssetIndex;
            Dest->DataOffset = ftell(Out);

            if (Source->Type == AssetType_Sound) {
                loaded_sound WAV = LoadWAV(Source->Sound.Filename, Source->Sound.FirstSampleIndex, Dest->Sound.SampleCount);
                Dest->Sound.SampleCount = WAV.SampleCount; // For the cases where it's zero
                Dest->Sound.ChannelCount = WAV.ChannelCount;
                for (uint32 ChannelIndex = 0; ChannelIndex < WAV.ChannelCount; ++ChannelIndex) {
                    fwrite(WAV.Samples[ChannelIndex], WAV.SampleCount * sizeof(int16), 1, Out);
                }
                free(WAV.Free);
            } else if (Source->Type == AssetType_Font) {
                uint32 CodepointsSize = sizeof(bitmap_id) * Source->Font.Font->CodepointCount;
                uint32 HorizontalAdvanceSize = sizeof(real32) * Source->Font.Font->CodepointCount * Source->Font.Font->CodepointCount;
                fwrite(Source->Font.Font->BitmapIDs, CodepointsSize, 1, Out);
                fwrite(Source->Font.Font->HorizontalAdvance, HorizontalAdvanceSize, 1, Out);
            } else {
                loaded_bitmap Bitmap;
                if (Source->Type == AssetType_FontGlyph) {
                    Bitmap = LoadGlyphBitmap(Source->Glyph.Font, Source->Glyph.Codepoint, Dest);
                } else {
                    Assert(Source->Type == AssetType_Bitmap);
                    Bitmap = LoadBMP(Source->Bitmap.Filename);
                }
                Dest->Bitmap.Dim[0] = Bitmap.Width;
                Dest->Bitmap.Dim[1] = Bitmap.Height;

                Assert(Bitmap.Width * 4 == Bitmap.Pitch);
                fwrite(Bitmap.Memory, Bitmap.Height * Bitmap.Width * BITMAP_BYTES_PER_PIXEL, 1, Out);
                free(Bitmap.Free);
            }

        }
        fseek(Out, (uint32)Header.Assets, SEEK_SET);
        fwrite(Assets->Assets, AssetsSize, 1, Out);
        fclose(Out);
    } else {
        printf("ERROR: Couldn't open file\n");
    }
}

internal void Initialize(game_assets* Assets) {
    Assets->TagCount = 1;
    Assets->AssetCount = 1;
    Assets->DEBUGAssetType = 0;
    Assets->AssetIndex = 0;

    Assets->AssetTypeCount = Asset_Count;
    memset(Assets->AssetTypes, 0, sizeof(Assets->AssetTypes));
}

internal void WriteHero() {
    game_assets Assets_;
    game_assets* Assets = &Assets_;
    Initialize(Assets);

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

    WriteHHA(Assets, "test1.hha");
}

internal void WriteNonHero() {
    game_assets Assets_;
    game_assets* Assets = &Assets_;
    Initialize(Assets);

    BeginAssetType(Assets, Asset_Shadow);
    AddBitmapAsset(Assets, "test/test_hero_shadow.bmp", 0.5f, 0.15668f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Tree);
    AddBitmapAsset(Assets, "test2/tree00.bmp", 0.494f, 0.295f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Sword);
    AddBitmapAsset(Assets, "test2/rock03.bmp", 0.5f, 0.656f);
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

    WriteHHA(Assets, "test2.hha");
}

internal void WriteSounds() {
    game_assets Assets_;
    game_assets* Assets = &Assets_;
    Initialize(Assets);

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
    for (uint32 FirstSampleIndex = 0; FirstSampleIndex < TotalMusicSampleCount; FirstSampleIndex += OneMusicChunk) {
        uint32 SampleCount = TotalMusicSampleCount - FirstSampleIndex;
        if (SampleCount > OneMusicChunk) {
            SampleCount = OneMusicChunk;
        }
        sound_id ThisMusic = AddSoundAsset(Assets, "test3/music_test.wav", FirstSampleIndex, SampleCount);
        if (FirstSampleIndex + OneMusicChunk < TotalMusicSampleCount) {
            Assets->Assets[ThisMusic.Value].Sound.Chain = HHASoundChain_Advance;
        }
    }
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Puhp);
    AddSoundAsset(Assets, "test3/puhp_00.wav");
    AddSoundAsset(Assets, "test3/puhp_01.wav");
    EndAssetType(Assets);

    WriteHHA(Assets, "test3.hha");
}

internal void WriteFonts() {
    game_assets Assets_;
    game_assets* Assets = &Assets_;
    Initialize(Assets);

    loaded_font DebugFont = LoadFont("C:/Windows/Fonts/arial.ttf", "Arial", '~' + 1);

    BeginAssetType(Assets, Asset_Font);
    AddFontAsset(Assets, &DebugFont);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_FontGlyph);
    for (uint32 Character = '!'; Character <= '~'; ++Character) {
        DebugFont.BitmapIDs[Character] = AddCharacterAsset(Assets, &DebugFont, Character);
    }
    EndAssetType(Assets);

    WriteHHA(Assets, "testfonts.hha");
}

int main(int ArgCount, char** Args) {
    InitlializeFontDC();
    WriteHero();
    WriteNonHero();
    WriteSounds();
    WriteFonts();
}
