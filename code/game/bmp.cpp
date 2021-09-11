#if !defined(HANDMADE_BMP_CPP)
#define HANDMADE_BMP_CPP

#include "../types.h"
#include "../intrinsics.h"
#include "lib.hpp"
#include "math.cpp"

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


struct loaded_bitmap {
    void* Memory;
    int32 Width;
    int32 Height;
    int32 Pitch;
    v2 AlignPercentage;
    real32 WidthOverHeight;
};

internal v4 SRGB255ToLinear1(v4 Color) {
    v4 Result;
    v4 Color01 = Color * (1.0f / 255.0f);
    Result.r = Square(Color01.r);
    Result.g = Square(Color01.g);
    Result.b = Square(Color01.b);
    Result.a = Color01.a;
    return Result;
}

internal v4 Linear1ToSRGB255(v4 Color) {
    v4 Result01;
    Result01.r = SquareRoot(Color.r);
    Result01.g = SquareRoot(Color.g);
    Result01.b = SquareRoot(Color.b);
    Result01.a = Color.a;
    v4 Result = Result01 * 255.0f;
    return Result;
}

internal v2 TopDownAlign(loaded_bitmap* Bitmap, v2 Align) {
    v2 Result = V2(Align.x, (Bitmap->Height - 1) - Align.y);
    Result.x = SafeRatio0(Result.x, (real32)Bitmap->Width);
    Result.y = SafeRatio0(Result.y, (real32)Bitmap->Height);
    return Result;
}

internal loaded_bitmap DEBUGLoadBMP(char* Filename, v2 AlignPercentage = V2(0.5f, 0.5f)) {
    debug_read_file_result ReadResult = DEBUGPlatformReadEntireFile(Filename);
    loaded_bitmap Result = {};
    if (ReadResult.Size == 0) {
        return Result;
    }
    bitmap_header* Header = (bitmap_header*)(ReadResult.Contents);

    Assert(Header->Compression == 3);

    Result.Height = Header->Height;
    Assert(Result.Height >= 0);
    Result.Width = Header->Width;
    Result.WidthOverHeight = SafeRatio0((real32)Result.Width, (real32)Result.Height);
    Result.Pitch = Result.Width * BITMAP_BYTES_PER_PIXEL;
    Result.AlignPercentage = AlignPercentage;

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
    return Result;
}

#endif
