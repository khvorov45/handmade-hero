#if !defined(HANDMADE_UTIL_H)
#define HANDMADE_UTIL_H

#define Pi32 3.14159265358979323846264338327950f

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Align16(Value) ((Value + 15) & ~15)
#define Align8(Value) ((Value + 7) & ~7)
#define Align4(Value) ((Value + 3) & ~3)
#define AlignPow2(Value, Alignment) (((Value) + ((Alignment) - 1)) & ~((Alignment) - 1))

#define Kilobytes(n) (((uint64)(n)) * 1024)
#define Megabytes(n) (Kilobytes(n) * 1024)
#define Gigabytes(n) (Megabytes(n) * 1024)
#define Terabytes(n) (Gigabytes(n) * 1024)

#define Minimum(A, B) ((A) < (B) ? (A) : (B))
#define Maximum(A, B) ((A) > (B) ? (A) : (B))

#if HANDMADE_SLOW
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath");
#define InvalidDefaultCase default: {InvalidCodePath;} break

inline uint32 SafeTruncateUint64(uint64 Value) {
    Assert(Value <= 0xFFFFFFFF);
    return (uint32)(Value);
}

inline uint16 SafeTruncateToUint16(int32 Value) {
    Assert(Value <= 65536);
    Assert(Value >= 0);
    return (uint16)(Value);
}

#endif
