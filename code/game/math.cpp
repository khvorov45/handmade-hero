#if !defined(HANDMADE_MATH_CPP)
#define HANDMADE_MATH_CPP

#include "../types.h"
#include "../intrinsics.h"

union v2 {
    struct { real32 X, Y; };
    real32 E[2];
};

union v3 {
    struct { real32 X, Y, Z; };
    struct { v2 XY; real32 Ignore_; };
    real32 E[3];
};

union v4 {
    struct { real32 X, Y, Z, W; };
    struct { real32 R, G, B, A; };
    real32 E[4];
};

inline v2 V2(real32 X, real32 Y) {
    v2 Result;
    Result.X = X;
    Result.Y = Y;
    return Result;
};

inline v3 V3(real32 X, real32 Y, real32 Z) {
    v3 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    return Result;
};

inline v3 V3(v2 XY, real32 Z) {
    v3 Result;
    Result.X = XY.X;
    Result.Y = XY.Y;
    Result.Z = Z;
    return Result;
};

inline v4 V4(real32 X, real32 Y, real32 Z, real32 W) {
    v4 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;
    return Result;
};

inline v2 operator-(v2 A) {
    v2 Result;
    Result.X = -A.X;
    Result.Y = -A.Y;
    return Result;
}

inline v3 operator-(v3 A) {
    v3 Result;
    Result.X = -A.X;
    Result.Y = -A.Y;
    Result.Z = -A.Z;
    return Result;
}

inline v2 operator*(real32 A, v2 B) {
    v2 Result;
    Result.X = B.X * A;
    Result.Y = B.Y * A;
    return Result;
}

inline v3 operator*(real32 A, v3 B) {
    v3 Result;
    Result.X = B.X * A;
    Result.Y = B.Y * A;
    Result.Z = B.Z * A;
    return Result;
}

inline v2 operator*(v2 B, real32 A) {
    return A * B;
}

inline v3 operator*(v3 B, real32 A) {
    return A * B;
}

inline v2& operator*=(v2& A, real32 B) {
    A = A * B;
    return A;
}

inline v3& operator*=(v3& A, real32 B) {
    A = A * B;
    return A;
}

inline v2 operator+(v2 A, v2 B) {
    v2 Result;
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    return Result;
}

inline v3 operator+(v3 A, v3 B) {
    v3 Result;
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    Result.Z = A.Z + B.Z;
    return Result;
}

inline v2& operator+=(v2& A, v2 B) {
    A = A + B;
    return A;
}

inline v3& operator+=(v3& A, v3 B) {
    A = A + B;
    return A;
}

inline v2 operator-(v2 A, v2 B) {
    v2 Result;
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    return Result;
}

inline v3 operator-(v3 A, v3 B) {
    v3 Result;
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    Result.Z = A.Z - B.Z;
    return Result;
}

inline real32 Inner(v2 A, v2 B) {
    return A.X * B.X + A.Y * B.Y;
}

inline real32 Inner(v3 A, v3 B) {
    return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
}

inline v3 Hadamard(v3 A, v3 B) {
    v3 Result = {};
    Result.X = A.X * B.X;
    Result.Y = A.Y * B.Y;
    Result.Z = A.Z * B.Z;
    return Result;
}

inline v2 Hadamard(v2 A, v2 B) {
    v2 Result = {};
    Result.X = A.X * B.X;
    Result.Y = A.Y * B.Y;
    return Result;
}

inline real32 LengthSq(v2 A) {
    return Inner(A, A);
}

inline real32 LengthSq(v3 A) {
    return Inner(A, A);
}

inline real32 Length(v2 A) {
    return SquareRoot(LengthSq(A));
}

inline real32 Length(v3 A) {
    return SquareRoot(LengthSq(A));
}

inline real32 Square(real32 A) {
    return A * A;
}

inline real32 Lerp(real32 A, real32 t, real32 B) {
    real32 Result = (1.0f - t) * A + t * B;
    return Result;
}

struct rectangle2 {
    v2 Min;
    v2 Max;
};

struct rectangle3 {
    v3 Min;
    v3 Max;
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

inline rectangle3 RectCenterHalfDim(v3 Center, v3 HalfDim) {
    rectangle3 Rect;
    Rect.Max = Center + HalfDim;
    Rect.Min = Center - HalfDim;
    return Rect;
}

inline rectangle2 RectCenterDim(v2 Center, v2 Dim) {
    return RectCenterHalfDim(Center, 0.5f * Dim);
}

inline rectangle3 RectCenterDim(v3 Center, v3 Dim) {
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

inline bool32 IsInRectangle(rectangle3 Rectangle, v3 Test) {
    bool32 Result = (
        (Test.X >= Rectangle.Min.X) &&
        (Test.Y >= Rectangle.Min.Y) &&
        (Test.Z >= Rectangle.Min.Z) &&
        (Test.X < Rectangle.Max.X) &&
        (Test.Y < Rectangle.Max.Y) &&
        (Test.Z < Rectangle.Max.Z)
        );
    return Result;
}

inline bool32 RectanglesIntersect(rectangle3 A, rectangle3 B) {
    bool32 Result = !(
        (B.Max.X < A.Min.X) || (B.Min.X > A.Max.X) ||
        (B.Max.Y < A.Min.Y) || (B.Min.Y > A.Max.Y) ||
        (B.Max.Z < A.Min.Z) || (B.Min.Z > A.Max.Z)
        );
    return Result;
}

inline v2 GetMinCorner(rectangle2 Rect) {
    return Rect.Min;
}

inline v3 GetMinCorner(rectangle3 Rect) {
    return Rect.Min;
}

inline v2 GetMaxCorner(rectangle2 Rect) {
    return Rect.Max;
}

inline v3 GetMaxCorner(rectangle3 Rect) {
    return Rect.Max;
}

inline v2 GetCenter(rectangle2 Rect) {
    return 0.5f * (Rect.Max + Rect.Min);
}

inline v3 GetCenter(rectangle3 Rect) {
    return 0.5f * (Rect.Max + Rect.Min);
}

inline rectangle2 AddRadius(rectangle2 Rect, v2 Radius) {
    Rect.Max = Rect.Max + Radius;
    Rect.Min = Rect.Min - Radius;
    return Rect;
}

inline rectangle3 AddRadius(rectangle3 Rect, v3 Radius) {
    Rect.Max = Rect.Max + Radius;
    Rect.Min = Rect.Min - Radius;
    return Rect;
}

inline real32 SafeRatioN(real32 Numerator, real32 Divisor, real32 N) {
    real32 Result = N;
    if (Divisor != 0.0f) {
        Result = Numerator / Divisor;
    }
    return Result;
}

inline real32 SafeRatio0(real32 Numerator, real32 Divisor) {
    return SafeRatioN(Numerator, Divisor, 0.0f);
}

inline real32 SafeRatio1(real32 Numerator, real32 Divisor) {
    return SafeRatioN(Numerator, Divisor, 1.0f);

}

inline v3 GetBarycentric(rectangle3 Rect, v3 P) {
    v3 Result = {};
    v3 Diff = Rect.Max - Rect.Min;
    Result.X = SafeRatio0(P.X - Rect.Min.X, Diff.X);
    Result.Y = SafeRatio0(P.Y - Rect.Min.Y, Diff.Y);
    Result.Z = SafeRatio0(P.Z - Rect.Min.Z, Diff.Z);
    return Result;
}

inline real32 Clamp(real32 Min, real32 Value, real32 Max) {
    real32 Result = Value;
    if (Result < Min) {
        Result = Min;
    } else if (Result > Max) {
        Result = Max;
    }
    return Result;
}

inline real32 Clamp01(real32 Value) {
    return Clamp(0, Value, 1);
}

inline v3 Clamp01(v3 Value) {
    v3 Result = Value;
    Result.X = Clamp01(Result.X);
    Result.Y = Clamp01(Result.Y);
    Result.Z = Clamp01(Result.Z);
    return Result;
}

#endif
