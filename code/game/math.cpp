#if !defined(HANDMADE_MATH_CPP)
#define HANDMADE_MATH_CPP

#include "../types.h"

struct v2 {
    union {
        struct { real32 X, Y; };
        real32 E[2];
    };
};

inline v2 V2(real32 X, real32 Y) {
    v2 Result;
    Result.X = X;
    Result.Y = Y;
    return Result;
};

inline v2 operator-(v2 A) {
    v2 Result;
    Result.X = -A.X;
    Result.Y = -A.Y;
    return Result;
}

inline v2 operator*(real32 A, v2 B) {
    v2 Result;
    Result.X = B.X * A;
    Result.Y = B.Y * A;
    return Result;
}

inline v2 operator*(v2 B, real32 A) {
    return A * B;
}

inline v2& operator*=(v2& A, real32 B) {
    A = A * B;
    return A;
}

inline v2 operator+(v2 A, v2 B) {
    v2 Result;
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    return Result;
}

inline v2& operator+=(v2& A, v2 B) {
    A = A + B;
    return A;
}

inline v2 operator-(v2 A, v2 B) {
    v2 Result;
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    return Result;
}

#endif
