#if !defined(HANDMADE_INTRINSICS_H)
#define HANDMADE_INTRINSICS_H

#include "types.h"
#include "math.h"

internal inline int32 RoundReal32ToInt32(real32 X) {
    return (int32)roundf(X);
}

internal inline uint32 RoundReal32ToUint32(real32 X) {
    return (uint32)roundf(X);
}

internal inline int32 TruncateReal32ToInt32(real32 Real32) {
    return (int32)Real32;
}

internal inline int32 FloorReal32ToInt32(real32 Real32) {
    return (int32)floorf(Real32);
}

internal inline real32 Sin(real32 Angle) {
    return sinf(Angle);
}

internal inline real32 Cos(real32 Angle) {
    return cosf(Angle);
}

internal inline real32 ATan2(real32 Y, real32 X) {
    return atan2f(Y, X);
}

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

#endif
