#if !defined(HANDMADE_BMP_CPP)
#define HANDMADE_BMP_CPP

#include "../types.h"
#include "lib.hpp"

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

struct bit_scan_result {
    bool32 Found;
    uint32 Index;
};

internal bit_scan_result FindLeastSignificantSetBit(uint32 Value) {
    bit_scan_result Result = {};
    for (uint32 Test = 0; Test < 32; ++Test) {
        if (Value & (1 << Test)) {
            Result.Index = Test;
            Result.Found = true;
            break;
        }
    }
    return Result;
}

struct loaded_bmp {
    uint32* Pixels;
    int32 Width;
    int32 Height;
};

internal loaded_bmp DEBUGLoadBMP(
    thread_context* Thread,
    debug_platform_read_entire_file* DEBUGPlatformReadEntireFile,
    char* Filename
) {
    debug_read_file_result ReadResult = DEBUGPlatformReadEntireFile(Thread, Filename);
    loaded_bmp Result = {};
    if (ReadResult.Size == 0) {
        return Result;
    }
    bitmap_header* Header = (bitmap_header*)(ReadResult.Contents);
    Result.Height = Header->Height;
    Result.Width = Header->Width;

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
            *Pixel++ =
                (((*Pixel >> AlphaShift) & 0xFF) << 24) |
                (((*Pixel >> RedShift) & 0xFF) << 16) |
                (((*Pixel >> GreenShift) & 0xFF) << 8) |
                (((*Pixel >> BlueShift) & 0xFF));
        }
    }
    Result.Pixels = Pixels;
    return Result;
}

#endif
