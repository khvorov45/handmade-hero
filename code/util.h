#if !defined(HANDMADE_UTIL_H)
#define HANDMADE_UTIL_H

#define Pi32 3.14159265358979323846264338327950f

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

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

inline uint32 SafeTruncateUint64(uint64 Value) {
    Assert(Value <= 0xFFFFFFFF);
    return (uint32)(Value);
}

#endif
