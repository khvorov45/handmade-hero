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
};
#pragma pack(pop)


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
    /*
    * Byte order is BB GG RR AA
    * Loaded as 0xAARRGGBB which is what we expect
    * Except not really because some files are actually AA BB GG RR, so the format is per-file
    * apparently.
    */
    uint32* Pixels = (uint32*)((uint8*)ReadResult.Contents + Header->BitmapOffset);
    uint32* Pixel = Pixels;
    for (int32 Y = 0; Y < Header->Height; ++Y) {
        for (int32 X = 0; X < Header->Width; ++X) {
            *Pixel++ = (*Pixel >> 8) | (*Pixel << 24);
        }
    }
    Result.Pixels = Pixels;
    return Result;
}

#endif
