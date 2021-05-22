#if !defined(HANDMADE_MATH_CPP)
#define HANDMADE_MATH_CPP

#include "../types.h"

union v2 {
    struct { real32 X, Y; };
    real32 E[2];
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

inline real32 Square(real32 A) {
    return A * A;
}

inline real32 Inner(v2 A, v2 B) {
    return A.X * B.X + A.Y * B.Y;
}

inline real32 LengthSq(v2 A) {
    return Inner(A, A);
}

struct rectangle2 {
    v2 Min;
    v2 Max;
};

inline rectangle2 RectMinMax(v2 Min, v2 Max) {
    rectangle2 Rect;
    Rect.Max = Max;
    Rect.Min = Min;
    return Rect;
}

inline rectangle2 RectMinDim(v2 Min, v2 Dim) {
    rectangle2 Rect;
    Rect.Max = Min + Dim;
    Rect.Min = Min;
    return Rect;
}

inline rectangle2 RectCenterHalfDim(v2 Center, v2 HalfDim) {
    rectangle2 Rect;
    Rect.Max = Center + HalfDim;
    Rect.Min = Center - HalfDim;
    return Rect;
}

inline rectangle2 RectCenterDim(v2 Center, v2 Dim) {
    return RectCenterHalfDim(Center, 0.5f * Dim);
}

inline bool32 IsInRectangle(rectangle2 Rectangle, v2 Test) {
    bool32 Result = (
        (Test.X >= Rectangle.Min.X) &&
        (Test.Y >= Rectangle.Min.Y) &&
        (Test.X < Rectangle.Max.X) &&
        (Test.Y < Rectangle.Max.Y)
        );
    return Result;
}

#endif
