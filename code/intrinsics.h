#if !defined(HANDMADE_INTRINSICS_H)
#define HANDMADE_INTRINSICS_H

#include "types.h"
#include "math.h"

#if COMPILER_MSVC
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()
#include <intrin.h>
#endif

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

internal inline int32 CeilReal32ToUint32(real32 Real32) {
    return (uint32)ceilf(Real32);
}

internal inline int32 CeilReal32ToInt32(real32 Real32) {
    return (int32)ceilf(Real32);
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

internal inline real32 AbsoluteValue(real32 X) {
    return fabsf(X);
}

struct bit_scan_result {
    bool32 Found;
    uint32 Index;
};

internal inline bit_scan_result FindLeastSignificantSetBit(uint32 Value) {
    bit_scan_result Result = {};

#if COMPILER_MSVC
    Result.Found = _BitScanForward((unsigned long*)&Result.Index, Value);
#else
    for (uint32 Test = 0; Test < 32; ++Test) {
        if (Value & (1 << Test)) {
            Result.Index = Test;
            Result.Found = true;
            break;
        }
    }
#endif
    return Result;
}

internal inline real32 SquareRoot(real32 X) {
    return sqrtf(X);
}

internal inline int32 SignOf(int32 X) {
    return X >= 0 ? 1 : -1;
}

#endif
