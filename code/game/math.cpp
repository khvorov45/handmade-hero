#if !defined(HANDMADE_MATH_CPP)
#define HANDMADE_MATH_CPP

#include "../types.h"
#include "../intrinsics.h"

union v2 {
    struct { real32 x, y; };
    real32 E[2];
};

union v3 {
    struct { real32 x, y, z; };
    struct { real32 r, g, b; };
    struct { v2 xy; real32 Ignore_; };
    real32 E[3];
};

union v4 {
    struct {
        union {
            v3 xyz;
            struct { real32 x, y, z; };
        };
        real32 w;
    };
    struct {
        union {
            v3 rgb;
            struct { real32 r, g, b; };
        };
        real32 a;
    };
    struct { v2 xy; real32 Ignore1, Ignore2; };
    real32 E[4];
};

inline v2 V2(real32 X, real32 Y) {
    v2 Result;
    Result.x = X;
    Result.y = Y;
    return Result;
};

inline v2 V2u(uint32 X, uint32 Y) {
    v2 Result = { (real32)X, (real32)Y };
    return Result;
}

inline v2 V2i(int32 X, int32 Y) {
    v2 Result = { (real32)X, (real32)Y };
    return Result;
}

inline v3 V3(real32 X, real32 Y, real32 Z) {
    v3 Result;
    Result.x = X;
    Result.y = Y;
    Result.z = Z;
    return Result;
};

inline v3 V3i(int32 X, int32 Y, int32 Z) {
    v3 Result;
    Result.x = (real32)X;
    Result.y = (real32)Y;
    Result.z = (real32)Z;
    return Result;
};

inline v3 V3(v2 XY, real32 Z) {
    v3 Result;
    Result.x = XY.x;
    Result.y = XY.y;
    Result.z = Z;
    return Result;
};

inline v4 V4(real32 X, real32 Y, real32 Z, real32 W) {
    v4 Result;
    Result.x = X;
    Result.y = Y;
    Result.z = Z;
    Result.w = W;
    return Result;
};

inline v4 V4(v3 A, real32 W) {
    v4 Result;
    Result.xyz = A;
    Result.w = W;
    return Result;
};

inline v2 operator-(v2 A) {
    v2 Result;
    Result.x = -A.x;
    Result.y = -A.y;
    return Result;
}

inline v3 operator-(v3 A) {
    v3 Result;
    Result.x = -A.x;
    Result.y = -A.y;
    Result.z = -A.z;
    return Result;
}

inline v2 operator*(real32 A, v2 B) {
    v2 Result;
    Result.x = B.x * A;
    Result.y = B.y * A;
    return Result;
}

inline v3 operator*(real32 A, v3 B) {
    v3 Result;
    Result.x = B.x * A;
    Result.y = B.y * A;
    Result.z = B.z * A;
    return Result;
}

inline v4 operator*(real32 A, v4 B) {
    v4 Result;
    Result.x = B.x * A;
    Result.y = B.y * A;
    Result.z = B.z * A;
    Result.w = B.w * A;
    return Result;
}

inline v2 operator*(v2 B, real32 A) {
    return A * B;
}

inline v3 operator*(v3 B, real32 A) {
    return A * B;
}

inline v4 operator*(v4 B, real32 A) {
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

inline v4& operator*=(v4& A, real32 B) {
    A = A * B;
    return A;
}

inline v2 operator+(v2 A, v2 B) {
    v2 Result;
    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    return Result;
}

inline v3 operator+(v3 A, v3 B) {
    v3 Result;
    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    Result.z = A.z + B.z;
    return Result;
}

inline v4 operator+(v4 A, v4 B) {
    v4 Result;
    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    Result.z = A.z + B.z;
    Result.w = A.w + B.w;
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

inline v4& operator+=(v4& A, v4 B) {
    A = A + B;
    return A;
}

inline v2 operator-(v2 A, v2 B) {
    v2 Result;
    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
    return Result;
}

inline v3 operator-(v3 A, v3 B) {
    v3 Result;
    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
    Result.z = A.z - B.z;
    return Result;
}

inline real32 Inner(v2 A, v2 B) {
    return A.x * B.x + A.y * B.y;
}

inline real32 Inner(v3 A, v3 B) {
    return A.x * B.x + A.y * B.y + A.z * B.z;
}

inline v4 Hadamard(v4 A, v4 B) {
    v4 Result;
    Result.x = A.x * B.x;
    Result.y = A.y * B.y;
    Result.z = A.z * B.z;
    Result.w = A.w * B.w;
    return Result;
}

inline v3 Hadamard(v3 A, v3 B) {
    v3 Result = {};
    Result.x = A.x * B.x;
    Result.y = A.y * B.y;
    Result.z = A.z * B.z;
    return Result;
}

inline v2 Hadamard(v2 A, v2 B) {
    v2 Result = {};
    Result.x = A.x * B.x;
    Result.y = A.y * B.y;
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

inline v3 Lerp(v3 A, real32 t, v3 B) {
    v3 Result = (1.0f - t) * A + t * B;
    return Result;
}

inline v4 Lerp(v4 A, real32 t, v4 B) {
    v4 Result = (1.0f - t) * A + t * B;
    return Result;
}

inline v3 Normalize(v3 A) {
    real32 InvLength = 1.0f / Length(A);
    v3 Result = A * InvLength;
    return Result;
}

struct rectangle2 {
    v2 Min;
    v2 Max;
};

struct rectangle2i {
    int32 MinX, MinY;
    int32 MaxX, MaxY;
};

struct rectangle3 {
    v3 Min;
    v3 Max;
};

internal inline rectangle2i InvertedInfinityRectangle() {
    rectangle2i Result;
    Result.MinX = Result.MinY = INT32_MAX;
    Result.MaxX = Result.MaxY = 0;
    return Result;
}

inline rectangle2 RectMinMax(v2 Min, v2 Max) {
    rectangle2 Rect;
    Rect.Max = Max;
    Rect.Min = Min;
    return Rect;
}

inline rectangle3 RectMinMax(v3 Min, v3 Max) {
    rectangle3 Rect;
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
        (Test.x >= Rectangle.Min.x) &&
        (Test.y >= Rectangle.Min.y) &&
        (Test.x < Rectangle.Max.x) &&
        (Test.y < Rectangle.Max.y)
        );
    return Result;
}

inline bool32 IsInRectangle(rectangle3 Rectangle, v3 Test) {
    bool32 Result = (
        (Test.x >= Rectangle.Min.x) &&
        (Test.y >= Rectangle.Min.y) &&
        (Test.z >= Rectangle.Min.z) &&
        (Test.x < Rectangle.Max.x) &&
        (Test.y < Rectangle.Max.y) &&
        (Test.z < Rectangle.Max.z)
        );
    return Result;
}

inline bool32 RectanglesIntersect(rectangle3 A, rectangle3 B) {
    bool32 Result = !(
        (B.Max.x <= A.Min.x) || (B.Min.x >= A.Max.x) ||
        (B.Max.y <= A.Min.y) || (B.Min.y >= A.Max.y) ||
        (B.Max.z <= A.Min.z) || (B.Min.z >= A.Max.z)
        );
    return Result;
}

internal inline rectangle2i Intersect(rectangle2i A, rectangle2i B) {
    rectangle2i Result = A;
    Result.MinX = (A.MinX < B.MinX) ? B.MinX : A.MinX;
    Result.MinY = (A.MinY < B.MinY) ? B.MinY : A.MinY;
    Result.MaxX = (A.MaxX > B.MaxX) ? B.MaxX : A.MaxX;
    Result.MaxY = (A.MaxY > B.MaxY) ? B.MaxY : A.MaxY;
    return Result;
}

internal inline rectangle2i Union(rectangle2i A, rectangle2i B) {
    rectangle2i Result = A;
    Result.MinX = (A.MinX < B.MinX) ? A.MinX : B.MinX;
    Result.MinY = (A.MinY < B.MinY) ? A.MinY : B.MinY;
    Result.MaxX = (A.MaxX > B.MaxX) ? A.MaxX : B.MaxX;
    Result.MaxY = (A.MaxY > B.MaxY) ? A.MaxY : B.MaxY;
    return Result;
}

internal inline int32 GetClampedRectArea(rectangle2i A) {
    uint32 Result = 0;
    if ((A.MaxX >= A.MinX) && (A.MaxY >= A.MinY)) {
        Result = (A.MaxX - A.MinX) * (A.MaxY - A.MinY);
    }
    return Result;
}

internal inline bool32 HasArea(rectangle2i A) {
    uint32 Result = (A.MaxX > A.MinX) && (A.MaxY > A.MinY);
    return Result;
}


inline v2 GetDim(rectangle2 Rect) {
    return Rect.Max - Rect.Min;
}

inline v3 GetDim(rectangle3 Rect) {
    return Rect.Max - Rect.Min;
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


inline rectangle3 Offset(rectangle3 Rect, v3 Offset) {
    rectangle3 Result;
    Result.Max = Rect.Max + Offset;
    Result.Min = Rect.Min + Offset;
    return Result;
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

inline v2 GetBarycentric(rectangle2 Rect, v2 P) {
    v2 Result = {};
    v2 Diff = Rect.Max - Rect.Min;
    Result.x = SafeRatio0(P.x - Rect.Min.x, Diff.x);
    Result.y = SafeRatio0(P.y - Rect.Min.y, Diff.y);
    return Result;
}

inline v3 GetBarycentric(rectangle3 Rect, v3 P) {
    v3 Result = {};
    v3 Diff = Rect.Max - Rect.Min;
    Result.x = SafeRatio0(P.x - Rect.Min.x, Diff.x);
    Result.y = SafeRatio0(P.y - Rect.Min.y, Diff.y);
    Result.z = SafeRatio0(P.z - Rect.Min.z, Diff.z);
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

inline v2 Clamp01(v2 Value) {
    v2 Result = Value;
    Result.x = Clamp01(Result.x);
    Result.y = Clamp01(Result.y);
    return Result;
}

inline v3 Clamp01(v3 Value) {
    v3 Result = Value;
    Result.x = Clamp01(Result.x);
    Result.y = Clamp01(Result.y);
    Result.z = Clamp01(Result.z);
    return Result;
}

inline v4 Clamp01(v4 Value) {
    v4 Result = Value;
    Result.x = Clamp01(Result.x);
    Result.y = Clamp01(Result.y);
    Result.z = Clamp01(Result.z);
    Result.w = Clamp01(Result.w);
    return Result;
}

internal inline real32 Clamp01MapToRange(real32 Min, real32 t, real32 Max) {
    real32 Result = 0.0f;
    real32 Range = Max - Min;
    if (Range != 0.0f) {
        Result = Clamp01((t - Min) / Range);
    }
    return Result;
}

inline rectangle2 ToRectangleXY(rectangle3 A) {
    rectangle2 Result;
    Result.Min = A.Min.xy;
    Result.Max = A.Max.xy;
    return Result;
}

inline v2 Perp(v2 A) {
    v2 Result = V2(-A.y, A.x);
    return Result;
}

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

#endif
