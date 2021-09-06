#include "math.cpp"
#include "bmp.cpp"
#include "sim_region.cpp"

struct render_basis {
    v3 P;
};

struct render_entity_basis {
    render_basis* Basis;
    v3 Offset;
};

enum render_group_entry_type {
    RenderGroupEntryType_render_entry_clear,
    RenderGroupEntryType_render_entry_bitmap,
    RenderGroupEntryType_render_entry_rectangle,
    RenderGroupEntryType_render_entry_coordinate_system,
    RenderGroupEntryType_render_entry_saturation
};

struct render_group_entry_header {
    render_group_entry_type Type;
};

struct render_entry_clear {
    v4 Color;
};

struct render_entry_saturation {
    real32 Level;
};

struct render_entry_coordinate_system {
    v2 Origin;
    v2 XAxis;
    v2 YAxis;
    v4 Color;
    loaded_bitmap* Texture;
    loaded_bitmap* NormalMap;
    environment_map* Top;
    environment_map* Middle;
    environment_map* Bottom;
};

struct render_entry_bitmap {
    loaded_bitmap* Bitmap;
    render_entity_basis EntityBasis;
    v2 Size;
    v4 Color;
};

struct render_entry_rectangle {
    render_entity_basis EntityBasis;
    v2 Dim;
    v4 Color;
};

struct render_group_camera {
    real32 FocalLength;
    real32 DistanceAboveTarget;
};

struct render_group {
    real32 GlobalAlpha;
    render_basis* DefaultBasis;
    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8* PushBufferBase;
    render_group_camera GameCamera;
    render_group_camera RenderCamera;
    real32 MetersToPixels;
    v2 MonitorHalfDimInMeters;
};

internal render_group* AllocateRenderGroup(
    memory_arena* Arena, uint32 MaxPushBufferSize,
    uint32 ResolutionPixelsX, uint32 ResulutionPixelsY
) {
    render_group* Result = PushStruct(Arena, render_group);
    Result->PushBufferBase = (uint8*)PushSize(Arena, MaxPushBufferSize);
    Result->DefaultBasis = PushStruct(Arena, render_basis);
    Result->DefaultBasis->P = V3(0, 0, 0);
    Result->MaxPushBufferSize = MaxPushBufferSize;
    Result->PushBufferSize = 0;
    Result->GlobalAlpha = 1.0f;
    Result->GameCamera.FocalLength = 0.6f;
    Result->GameCamera.DistanceAboveTarget = 9.0f;
    Result->RenderCamera = Result->GameCamera;
    // Result->RenderCamera.DistanceAboveTarget = 30.0f;
    real32 WidthOfMonitor = 0.635f;
    Result->MetersToPixels = (real32)(ResolutionPixelsX)*WidthOfMonitor; // NOTE(sen) Should be a division
    real32 PixelsToMeters = 1.0f / Result->MetersToPixels;
    Result->MonitorHalfDimInMeters = 0.5f * V2u(ResolutionPixelsX, ResulutionPixelsY) * PixelsToMeters;
    return Result;
}

struct entity_basis_p_result {
    v2 P;
    real32 Scale;
    bool32 Valid;
};

internal entity_basis_p_result GetRenderEntityBasisP(
    render_group* RenderGroup, render_entity_basis* EntityBasis, v2 ScreenDim
) {
    v2 ScreenCenter = 0.5f * ScreenDim;
    entity_basis_p_result Result = {};
    v3 EntityBaseP = EntityBasis->Basis->P;
    // NOTE(sen) +Z = out of screen
    real32 DistanceToPZ = RenderGroup->RenderCamera.DistanceAboveTarget - EntityBaseP.z;
    v3 RawXY = V3(EntityBaseP.xy + EntityBasis->Offset.xy, 1.0f);
    real32 NearClipPlane = 0.2f;
    if (DistanceToPZ > NearClipPlane) {
        v3 ProjectedXY = RawXY * (RenderGroup->RenderCamera.FocalLength / DistanceToPZ);
        Result.P = ScreenCenter + RenderGroup->MetersToPixels * ProjectedXY.xy;
        Result.Valid = true;
        Result.Scale = ProjectedXY.z * RenderGroup->MetersToPixels;
    }
    return Result;
}

#define PushRenderElement(Group, type) (type*)PushRenderElement_(Group, sizeof(type), RenderGroupEntryType_##type)

inline void*
PushRenderElement_(render_group* Group, uint32 Size, render_group_entry_type Type) {
    void* Result = 0;
    Size += sizeof(render_group_entry_header);
    if (Group->PushBufferSize + Size < Group->MaxPushBufferSize) {
        render_group_entry_header* Header =
            (render_group_entry_header*)(Group->PushBufferBase + Group->PushBufferSize);
        Header->Type = Type;
        Result = (uint8*)Header + sizeof(render_group_entry_header);
        Group->PushBufferSize += Size;
    } else {
        InvalidCodePath;
    }
    return Result;
}

internal inline void PushBitmap(
    render_group* Group, loaded_bitmap* Bitmap, real32 Height,
    v3 Offset, v4 Color = V4(1, 1, 1, 1)
) {
    render_entry_bitmap* Piece = PushRenderElement(Group, render_entry_bitmap);
    if (Piece) {
        Piece->EntityBasis.Basis = Group->DefaultBasis;
        Piece->Color = Color * Group->GlobalAlpha;
        Piece->Bitmap = Bitmap;
        Piece->Size = V2(Height * Bitmap->WidthOverHeight, Height);
        v2 Align = Hadamard(Bitmap->AlignPercentage, Piece->Size);
        Piece->EntityBasis.Offset = Offset - V3(Align, 0);
    }
}

internal inline void PushRect(
    render_group* Group,
    v3 Offset, v2 Dim,
    v4 Color = V4(1, 1, 1, 1)
) {
    render_entry_rectangle* Piece = PushRenderElement(Group, render_entry_rectangle);
    if (Piece) {
        Piece->EntityBasis.Basis = Group->DefaultBasis;
        Piece->Color = Color;
        Piece->EntityBasis.Offset = (Offset - V3(0.5f * Dim, 0));
        Piece->Dim = Dim;
    }
}

internal void Clear(render_group* Group, v4 Color) {
    render_entry_clear* Entry = PushRenderElement(Group, render_entry_clear);
    if (Entry) {
        Entry->Color = Color;
    }
}

internal void Saturation(render_group* Group, real32 Level) {
    render_entry_saturation* Entry = PushRenderElement(Group, render_entry_saturation);
    if (Entry) {
        Entry->Level = Level;
    }
}

internal void PushRectOutline(
    render_group* Group,
    v3 Offset, v2 Dim,
    v4 Color = V4(1, 1, 1, 1)
) {
    real32 Thickness = 0.1f;
#if 0
    //* Top and bottom
    PushRect(Group, Offset - V3(0, 0.5f * Dim.y, 0), V2(Dim.x, Thickness), Color);
    PushRect(Group, Offset + V3(0, 0.5f * Dim.y, 0), V2(Dim.x, Thickness), Color);

    //* Left and right
    PushRect(Group, Offset - V3(0.5f * Dim.x, 0, 0), V2(Thickness, Dim.y), Color);
    PushRect(Group, Offset + V3(0.5f * Dim.x, 0, 0), V2(Thickness, Dim.y), Color);
#endif
}

internal void CoordinateSystem(
    render_group* Group, v2 Origin, v2 XAxis, v2 YAxis, v4 Color, loaded_bitmap* Texture,
    loaded_bitmap* NormalMap, environment_map* Top, environment_map* Middle, environment_map* Bottom
) {
    render_entry_coordinate_system* Entry = PushRenderElement(Group, render_entry_coordinate_system);
    if (Entry) {
        Entry->Origin = Origin;
        Entry->XAxis = XAxis;
        Entry->YAxis = YAxis;
        Entry->Color = Color;
        Entry->Texture = Texture;
        Entry->NormalMap = NormalMap;
        Entry->Top = Top;
        Entry->Middle = Middle;
        Entry->Bottom = Bottom;
    }
}

internal v2 Unproject(render_group* Group, v2 ProjectedXY, real32 DistanceFromCamera) {
    v2 Result = ProjectedXY * (DistanceFromCamera / Group->GameCamera.FocalLength);
    return Result;
}

internal rectangle2 GetCameraRectagleAtDistance(
    render_group* Group, real32 DistanceFromCamera
) {
    v2 RawXY = Unproject(Group, Group->MonitorHalfDimInMeters, DistanceFromCamera);
    rectangle2 Result = RectCenterHalfDim(V2(0, 0), RawXY);
    return Result;
}

internal rectangle2
GetCameraRectagleAtTarget(render_group* Group) {
    rectangle2 Result = GetCameraRectagleAtDistance(Group, Group->GameCamera.DistanceAboveTarget);
    return Result;
}

/// Maximums are not inclusive
internal void DrawRectangle(
    loaded_bitmap* Buffer,
    v2 vMin, v2 vMax,
    v4 Color,
    rectangle2i ClipRect, bool32 Even
) {
    rectangle2i FillRect;
    FillRect.MinX = RoundReal32ToInt32(vMin.x);
    FillRect.MinY = RoundReal32ToInt32(vMin.y);
    FillRect.MaxX = RoundReal32ToInt32(vMax.x);
    FillRect.MaxY = RoundReal32ToInt32(vMax.y);

    FillRect = Intersect(FillRect, ClipRect);
    if (!Even == (FillRect.MinY & 1)) {
        FillRect.MinY += 1;
    }

    uint32 Color32 =
        (RoundReal32ToUint32(Color.a * 255.0f) << 24) |
        (RoundReal32ToUint32(Color.r * 255.0f) << 16) |
        (RoundReal32ToUint32(Color.g * 255.0f) << 8) |
        (RoundReal32ToUint32(Color.b * 255.0f));

    uint8* Row = (uint8*)Buffer->Memory + FillRect.MinY * Buffer->Pitch + FillRect.MinX * BITMAP_BYTES_PER_PIXEL;
    for (int32 Y = FillRect.MinY; Y < FillRect.MaxY; Y += 2) {
        uint32* Pixel = (uint32*)Row;
        for (int32 X = FillRect.MinX; X < FillRect.MaxX; ++X) {
            *Pixel++ = Color32;
        }
        Row += 2 * Buffer->Pitch;
    }
}

internal v4 Unpack4x8(uint32 Packed) {
    v4 Result = V4(
        (real32)((Packed >> 16) & 0xFF),
        (real32)((Packed >> 8) & 0xFF),
        (real32)((Packed) & 0xFF),
        (real32)((Packed >> 24))
    );
    return Result;
}

struct bilinear_sample {
    uint32 A, B, C, D;
};

internal bilinear_sample BilinearSample(loaded_bitmap* Texture, int32 X, int32 Y) {
    uint8* TexelPtr = (uint8*)Texture->Memory + Y * Texture->Pitch + X * BITMAP_BYTES_PER_PIXEL;
    bilinear_sample Result;
    Result.A = *(uint32*)TexelPtr;
    Result.B = *(uint32*)(TexelPtr + BITMAP_BYTES_PER_PIXEL);
    Result.C = *(uint32*)(TexelPtr + Texture->Pitch);
    Result.D = *(uint32*)(TexelPtr + Texture->Pitch + BITMAP_BYTES_PER_PIXEL);
    return Result;
}

internal v4 SRGBBilinearBlend(bilinear_sample TexelSample, real32 fX, real32 fY) {
    v4 TexelAv4 = Unpack4x8(TexelSample.A);
    v4 TexelBv4 = Unpack4x8(TexelSample.B);
    v4 TexelCv4 = Unpack4x8(TexelSample.C);
    v4 TexelDv4 = Unpack4x8(TexelSample.D);

    v4 TexelA01 = SRGB255ToLinear1(TexelAv4);
    v4 TexelB01 = SRGB255ToLinear1(TexelBv4);
    v4 TexelC01 = SRGB255ToLinear1(TexelCv4);
    v4 TexelD01 = SRGB255ToLinear1(TexelDv4);

    v4 Result = Lerp(
        Lerp(TexelA01, fX, TexelB01),
        fY,
        Lerp(TexelC01, fX, TexelD01)
    );
    return Result;
}

internal v3 SampleEnvironmentMap(
    v2 ScreenSpaceUV, v3 SampleDirection, real32 Roughness, environment_map* Map,
    real32 DistanceFromMapInZ
) {
    uint32 LODIndex = RoundReal32ToUint32(Roughness * (real32)(ArrayCount(Map->LOD) - 1));
    Assert(LODIndex < ArrayCount(Map->LOD));
    loaded_bitmap* LOD = Map->LOD + LODIndex;

    real32 UVsPerMeter = 0.1f;
    real32 Coef = UVsPerMeter * DistanceFromMapInZ / SampleDirection.y;
    v2 Offset = Coef * V2(SampleDirection.x, SampleDirection.z);
    v2 UV = Offset + ScreenSpaceUV;

    UV.x = Clamp01(UV.x);
    UV.y = Clamp01(UV.y);

    real32 MapXreal = UV.x * (real32)(LOD->Width - 2);
    real32 MapYreal = UV.y * (real32)(LOD->Height - 2);

    int32 MapXFloored = FloorReal32ToInt32(MapXreal);
    int32 MapYFloored = FloorReal32ToInt32(MapYreal);

    real32 fX = MapXreal - (real32)MapXFloored;
    real32 fY = MapYreal - (real32)MapYFloored;

    Assert(MapXFloored >= 0 && MapXFloored < LOD->Width);
    Assert(MapYFloored >= 0 && MapYFloored < LOD->Height);

#if 0
    uint8* TexelPtr = (uint8*)LOD->Memory + MapYFloored * LOD->Pitch + MapXFloored * BITMAP_BYTES_PER_PIXEL;
    *(uint32*)TexelPtr = 0xFFFFFFFF;
#endif

    bilinear_sample Sample = BilinearSample(LOD, MapXFloored, MapYFloored);
    v4 Result = SRGBBilinearBlend(Sample, fX, fY);
    return Result.xyz;
}

internal v4 UnscaleAndBiasNormal(v4 Normal) {
    v4 Result;
    real32 Inv255 = 1.0f / 255.0f;
    Result.x = -1.0f + 2.0f * Inv255 * Normal.x;
    Result.y = -1.0f + 2.0f * Inv255 * Normal.y;
    Result.z = -1.0f + 2.0f * Inv255 * Normal.z;
    Result.w = Inv255 * Normal.w;
    return Result;
}
unsigned long long __rdtsc();
internal void DrawRectangleSlowly(
    loaded_bitmap* Buffer,
    v2 Origin, v2 XAxis, v2 YAxis,
    v4 Color, loaded_bitmap* Texture, loaded_bitmap* NormalMap,
    environment_map* Top, environment_map* Middle, environment_map* Bottom,
    real32 PixelsToMeters
) {
    BEGIN_TIMED_BLOCK(DrawRectangleSlowly);

    Color.rgb *= Color.a;

    real32 XAxisLength = Length(XAxis);
    real32 YAxisLength = Length(YAxis);

    v2 NxAxis = (YAxisLength / XAxisLength) * XAxis;
    v2 NyAxis = (XAxisLength / YAxisLength) * YAxis;
    real32 NzScale = 0.5f * (XAxisLength + YAxisLength);

    int32 WidthMax = Buffer->Width - 1;
    int32 HeightMax = Buffer->Height - 1;
    int32 XMin = WidthMax;
    int32 XMax = 0;
    int32 YMin = HeightMax;
    int32 YMax = 0;

    real32 InvWidthMax = 1.0f / (real32)WidthMax;
    real32 InvHeightMax = 1.0f / (real32)HeightMax;

    real32 OriginZ = 0.0f;
    real32 OriginY = (Origin + 0.5f * XAxis + 0.5f * YAxis).y;
    real32 FixedCastY = InvHeightMax * OriginY;

    v2 P[4] = {
        Origin,
        Origin + XAxis,
        Origin + XAxis + YAxis,
        Origin + YAxis
    };

    for (uint32 PIndex = 0; PIndex < ArrayCount(P); PIndex++) {
        v2 TestP = P[PIndex];
        int32 FloorX = FloorReal32ToInt32(TestP.x);
        int32 CeilX = CeilReal32ToInt32(TestP.x);
        int32 FloorY = FloorReal32ToInt32(TestP.y);
        int32 CeilY = CeilReal32ToInt32(TestP.y);
        if (XMin > FloorX) {
            XMin = FloorX;
        }
        if (XMax < CeilX) {
            XMax = CeilX;
        }
        if (YMin > FloorY) {
            YMin = FloorY;
        }
        if (YMax < CeilY) {
            YMax = CeilY;
        }
    }

    if (XMin < 0) {
        XMin = 0;
    }
    if (YMin < 0) {
        YMin = 0;
    }
    if (XMax > WidthMax) {
        XMax = WidthMax;
    }
    if (YMax > HeightMax) {
        YMax = HeightMax;
    }

    real32 InvXAxisLengthSq = 1 / LengthSq(XAxis);
    real32 InvYAxisLengthSq = 1 / LengthSq(YAxis);

    uint8* Row = (uint8*)Buffer->Memory + YMin * Buffer->Pitch + XMin * BITMAP_BYTES_PER_PIXEL;
    BEGIN_TIMED_BLOCK(ProcessPixel);
    for (int32 Y = YMin; Y <= YMax; ++Y) {
        uint32* Pixel = (uint32*)Row;
        for (int32 X = XMin; X <= XMax; ++X) {

            v2 PixelP = V2i(X, Y);
            v2 d = PixelP - Origin;
            real32 Edge0 = Inner(d, -Perp(XAxis));
            real32 Edge1 = Inner(d - XAxis, -Perp(YAxis));
            real32 Edge2 = Inner(d - XAxis - YAxis, Perp(XAxis));
            real32 Edge3 = Inner(d - YAxis, Perp(YAxis));
            if (Edge0 < 0 && Edge1 < 0 && Edge2 < 0 && Edge3 < 0) {

#if 1
                v2 ScreenSpaceUV = V2((real32)X * InvWidthMax, FixedCastY);
                real32 ZDiff = PixelsToMeters * ((real32)Y - OriginY);
#else
                v2 ScreenSpaceUV = V2((real32)X * InvWidthMax, (real32)Y * InvHeightMax);
                real32 ZDiff = 0.0f;
#endif

                real32 U = Inner(d, XAxis) * InvXAxisLengthSq;
                real32 V = Inner(d, YAxis) * InvYAxisLengthSq;
#if 0
                Assert(U >= 0.0f && U <= 1.0f);
                Assert(V >= 0.0f && V <= 1.0f);
#endif
                real32 TextureX = U * (real32)(Texture->Width - 2);
                real32 TextureY = V * (real32)(Texture->Height - 2);

                int32 TextureXFloored = FloorReal32ToInt32(TextureX);
                int32 TextureYFloored = FloorReal32ToInt32(TextureY);

                real32 TextureXf = TextureX - (real32)TextureXFloored;
                real32 TextureYf = TextureY - (real32)TextureYFloored;

                Assert(TextureXFloored >= 0 && TextureXFloored < Texture->Width);
                Assert(TextureYFloored >= 0 && TextureYFloored < Texture->Height);

                bilinear_sample TexelSample =
                    BilinearSample(Texture, TextureXFloored, TextureYFloored);

                v4 Texel = SRGBBilinearBlend(TexelSample, TextureXf, TextureYf);

#if 0
                if (NormalMap) {
                    bilinear_sample NormalSample =
                        BilinearSample(NormalMap, TextureXFloored, TextureYFloored);

                    v4 NormalAv4 = Unpack4x8(NormalSample.A);
                    v4 NormalBv4 = Unpack4x8(NormalSample.B);
                    v4 NormalCv4 = Unpack4x8(NormalSample.C);
                    v4 NormalDv4 = Unpack4x8(NormalSample.D);

                    v4 Normal = Lerp(
                        Lerp(NormalAv4, TextureXf, NormalBv4),
                        TextureYf,
                        Lerp(NormalCv4, TextureXf, NormalDv4)
                    );

                    Normal = UnscaleAndBiasNormal(Normal);

                    Normal.xy = Normal.x * NxAxis + Normal.y * NyAxis;
                    Normal.z *= NzScale;

                    Normal.xyz = Normalize(Normal.xyz);

                    v3 BounceDirection = 2.0f * Normal.z * Normal.xyz;
                    BounceDirection.z -= 1.0f;
                    BounceDirection.z = -BounceDirection.z;
#if 1
                    environment_map* FarMap = 0;
                    real32 Pz = OriginZ + ZDiff;
                    real32 MapZ = 2.0f;
                    real32 tFarMap = 0.0f;
                    real32 tEnvMap = BounceDirection.y;
                    if (tEnvMap < -0.5f) {
                        FarMap = Bottom;
                        tFarMap = -2.0f * tEnvMap - 1.0f;
                    } else if (tEnvMap > 0.5f) {
                        FarMap = Top;
                        tFarMap = (tEnvMap - 0.5f) * 2.0f;
                    }
                    tFarMap *= tFarMap;

                    v3 LightColor = V3(0, 0, 0);
                    //SampleEnvironmentMap(ScreenSpaceUV, Normal.xyz, Normal.w, Middle);
                    if (FarMap) {
                        real32 DistanceFromMapInZ = FarMap->Pz - Pz;
                        v3 FarMapColor = SampleEnvironmentMap(
                            ScreenSpaceUV, BounceDirection, Normal.w, FarMap, DistanceFromMapInZ
                        );
                        LightColor = Lerp(LightColor, tFarMap, FarMapColor);
                    }
                    Texel.rgb = Texel.rgb + Texel.a * LightColor;
#if 0
                    Texel.rgb = V3(0.5f, 0.5f, 0.5f) + 0.5f * BounceDirection;
                    Texel.rgb *= Texel.a;
#endif
#else
                    Texel.rgb = V3(0.5f, 0.5f, 0.5f) + (0.5f * BounceDirection);
                    Texel.b = 0.0f;
                    Texel.r = 0.0f;
                    if (BounceDirection.y < 0.1 && BounceDirection.y > -0.1) {
                        Texel.rgb = V3(1.0f, 1.0f, 1.0f);
                    } else {
                        Texel.rgb = V3(0.0f, 0.0f, 0.0f);
                    }
#endif
                }
#endif
                Texel = Hadamard(Texel, Color);
                Texel.r = Clamp01(Texel.r);
                Texel.g = Clamp01(Texel.g);
                Texel.b = Clamp01(Texel.b);

                v4 Dest = V4(
                    (real32)((*Pixel >> 16) & 0xFF),
                    (real32)((*Pixel >> 8) & 0xFF),
                    (real32)((*Pixel) & 0xFF),
                    (real32)((*Pixel >> 24))
                );
                Dest = SRGB255ToLinear1(Dest);

                v4 Blended = (1.0f - Texel.a) * Dest + Texel;

                v4 Blended255 = Linear1ToSRGB255(Blended);

                *Pixel =
                    (RoundReal32ToUint32(Blended255.a) << 24) |
                    (RoundReal32ToUint32(Blended255.r) << 16) |
                    (RoundReal32ToUint32(Blended255.g) << 8) |
                    (RoundReal32ToUint32(Blended255.b));

            }
            Pixel++;
        }
        Row += Buffer->Pitch;
    }
    END_TIMED_BLOCK_COUNTED(ProcessPixel, (XMax - XMin + 1) * (YMax - YMin + 1));
    END_TIMED_BLOCK(DrawRectangleSlowly);
}

#include <xmmintrin.h>
#include <emmintrin.h>

#if 1
#include "../../iacaMarks.h"
#else
#define IACA_VC64_START
#define IACA_VC64_END
#endif

internal void DrawRectangleQuickly(
    loaded_bitmap* Buffer,
    v2 Origin, v2 XAxis, v2 YAxis,
    v4 Color, loaded_bitmap* Texture,
    real32 PixelsToMeters,
    rectangle2i ClipRect, bool32 Even
) {
    BEGIN_TIMED_BLOCK(DrawRectangleQuickly);

    Color.rgb *= Color.a;

    real32 XAxisLength = Length(XAxis);
    real32 YAxisLength = Length(YAxis);

    v2 NxAxis = (YAxisLength / XAxisLength) * XAxis;
    v2 NyAxis = (XAxisLength / YAxisLength) * YAxis;
    real32 NzScale = 0.5f * (XAxisLength + YAxisLength);

    int32 WidthMax = Buffer->Width - 3;
    int32 HeightMax = Buffer->Height - 3;

    rectangle2i FillRect = InvertedInfinityRectangle();

    v2 P[4] = {
        Origin,
        Origin + XAxis,
        Origin + XAxis + YAxis,
        Origin + YAxis
    };
    for (uint32 PIndex = 0; PIndex < ArrayCount(P); PIndex++) {
        v2 TestP = P[PIndex];
        int32 FloorX = FloorReal32ToInt32(TestP.x);
        int32 CeilX = CeilReal32ToInt32(TestP.x) + 1;
        int32 FloorY = FloorReal32ToInt32(TestP.y);
        int32 CeilY = CeilReal32ToInt32(TestP.y) + 1;
        if (FillRect.MinX > FloorX) {
            FillRect.MinX = FloorX;
        }
        if (FillRect.MaxX < CeilX) {
            FillRect.MaxX = CeilX;
        }
        if (FillRect.MinY > FloorY) {
            FillRect.MinY = FloorY;
        }
        if (FillRect.MaxY < CeilY) {
            FillRect.MaxY = CeilY;
        }
    }

    FillRect = Intersect(FillRect, ClipRect);

    // NOTE(sen) Alternating scanlines
    if (!Even == (FillRect.MinY & 1)) {
        FillRect.MinY += 1;
    }

    if (!HasArea(FillRect)) {
        return;
    }

    __m128i StartClipMask = _mm_set1_epi32(-1);
    __m128i EndClipMask = _mm_set1_epi32(-1);
    __m128i StartClipMasks[4] = {
        StartClipMask,
        _mm_slli_si128(StartClipMask, 1 * 4),
        _mm_slli_si128(StartClipMask, 2 * 4),
        _mm_slli_si128(StartClipMask, 3 * 4)
    };
    __m128i EndClipMasks[4] = {
        EndClipMask,
        _mm_srli_si128(EndClipMask, 3 * 4),
        _mm_srli_si128(EndClipMask, 2 * 4),
        _mm_srli_si128(EndClipMask, 1 * 4)
    };
    if (FillRect.MinX & 3) {
        StartClipMask = StartClipMasks[FillRect.MinX & 3];
        FillRect.MinX = FillRect.MinX & ~3;
    }
    if (FillRect.MaxX & 3) {
        EndClipMask = EndClipMasks[FillRect.MaxX & 3];
        FillRect.MaxX = (FillRect.MaxX & ~3) + 4;
    }

    real32 InvXAxisLengthSq = 1 / LengthSq(XAxis);
    real32 InvYAxisLengthSq = 1 / LengthSq(YAxis);

    v2 nXAxis = XAxis * InvXAxisLengthSq;
    v2 nYAxis = YAxis * InvYAxisLengthSq;

    __m128 One255_4x = _mm_set1_ps(255.0f);
    real32 Inv255 = 1.0f / 255.0f;
    __m128 Inv255_4x = _mm_set1_ps(Inv255);
    __m128 One_4x = _mm_set1_ps(1.0f);
    __m128 Two_4x = _mm_set1_ps(2.0f);
    __m128 Zero_4x = _mm_set1_ps(0.0f);
    __m128 Four_4x = _mm_set1_ps(4.0f);
    __m128i MaskFF_4x = _mm_set1_epi32(0xFF);
    __m128i MaskFFFF_4x = _mm_set1_epi32(0xFFFF);
    __m128i MaskFF00FF_4x = _mm_set1_epi32(0x00FF00FF);
    __m128i MaskFF00FF00_4x = _mm_set1_epi32(0xFF00FF00);
    __m128 MaxColorValue_4x = _mm_set1_ps(255.0f * 255.0f);

    real32 NormalizeC = 1 / 255.0f;
    real32 NormalizeCSq = Square(NormalizeC);
    __m128 Colorr_4x = _mm_set1_ps(Color.r);
    __m128 Colorg_4x = _mm_set1_ps(Color.b);
    __m128 Colorb_4x = _mm_set1_ps(Color.g);
    __m128 Colora_4x = _mm_set1_ps(Color.a);

    __m128 nXAxisx_4x = _mm_set1_ps(nXAxis.x);
    __m128 nXAxisy_4x = _mm_set1_ps(nXAxis.y);
    __m128 nYAxisx_4x = _mm_set1_ps(nYAxis.x);
    __m128 nYAxisy_4x = _mm_set1_ps(nYAxis.y);

    __m128 Originx_4x = _mm_set1_ps(Origin.x);
    __m128 Originy_4x = _mm_set1_ps(Origin.y);

    __m128 WidthM2 = _mm_set1_ps((real32)Texture->Width - 2);
    __m128 HeightM2 = _mm_set1_ps((real32)Texture->Height - 2);

    uint8* TextureMemory = (uint8*)Texture->Memory;
    int32 TexturePitch = Texture->Pitch;
    __m128i TexturePitch_4x = _mm_set1_epi32(TexturePitch);

    uint8* Row = (uint8*)Buffer->Memory + FillRect.MinY * Buffer->Pitch + FillRect.MinX * BITMAP_BYTES_PER_PIXEL;
    int32 RowAdvance = Buffer->Pitch * 2;

    int32 MaxY = FillRect.MaxY;
    int32 MinY = FillRect.MinY;
    int32 MaxX = FillRect.MaxX;
    int32 MinX = FillRect.MinX;

    BEGIN_TIMED_BLOCK(ProcessPixel);
    for (int32 Y = MinY; Y < MaxY; Y += 2) {

        uint32* Pixel = (uint32*)Row;
        __m128 PixelPy = _mm_set1_ps((real32)Y);
        PixelPy = _mm_sub_ps(PixelPy, Originy_4x);
        __m128 PynX = _mm_mul_ps(PixelPy, nXAxisy_4x);
        __m128 PynY = _mm_mul_ps(PixelPy, nYAxisy_4x);

        __m128 PixelPx = _mm_set_ps(
            (real32)(MinX + 3),
            (real32)(MinX + 2),
            (real32)(MinX + 1),
            (real32)(MinX + 0)
        );
        PixelPx = _mm_sub_ps(PixelPx, Originx_4x);

        __m128i ClipMask = StartClipMask;

        for (int32 XI = MinX; XI < MaxX; XI += 4, PixelPx = _mm_add_ps(PixelPx, Four_4x)) {

#define M(a, i) (((real32*)(&(a)))[i])
#define Mi(a, i) (((uint32*)(&(a)))[i])
#define mmSquare(a) _mm_mul_ps((a), (a))

            IACA_VC64_START;
            __m128 U = _mm_add_ps(_mm_mul_ps(PixelPx, nXAxisx_4x), PynX);
            __m128 V = _mm_add_ps(_mm_mul_ps(PixelPx, nYAxisx_4x), PynY);

            __m128i OriginalDest = _mm_load_si128((__m128i*)Pixel);
            __m128i WriteMask = _mm_castps_si128(_mm_and_ps(
                _mm_and_ps(_mm_cmpge_ps(U, Zero_4x), _mm_cmple_ps(U, One_4x)),
                _mm_and_ps(_mm_cmpge_ps(U, Zero_4x), _mm_cmple_ps(U, One_4x))
            ));
            WriteMask = _mm_and_si128(WriteMask, ClipMask);

            U = _mm_min_ps(_mm_max_ps(U, Zero_4x), One_4x);
            V = _mm_min_ps(_mm_max_ps(V, Zero_4x), One_4x);

            __m128 TextureX = _mm_mul_ps(U, WidthM2);
            __m128 TextureY = _mm_mul_ps(V, HeightM2);

            __m128i TextureXFloored = _mm_cvttps_epi32(TextureX);
            __m128i TextureYFloored = _mm_cvttps_epi32(TextureY);

            __m128 TextureXf = _mm_sub_ps(TextureX, _mm_cvtepi32_ps(TextureXFloored));
            __m128 TextureYf = _mm_sub_ps(TextureY, _mm_cvtepi32_ps(TextureYFloored));
#if 1


#if 1
            // NOTE(sen) mul by BITMAP_BYTES_PER_PIXEL
            __m128i FetchX = _mm_slli_epi32(TextureXFloored, 2);
            __m128i FetchY = _mm_or_si128(
                _mm_mullo_epi16(TextureYFloored, TexturePitch_4x),
                _mm_slli_epi32(_mm_mulhi_epi16(TextureYFloored, TexturePitch_4x), 16)
            );
            __m128i Fetch_4x = _mm_add_epi32(FetchX, FetchY);

            int32 Fetch0 = Mi(Fetch_4x, 0);
            int32 Fetch1 = Mi(Fetch_4x, 1);
            int32 Fetch2 = Mi(Fetch_4x, 2);
            int32 Fetch3 = Mi(Fetch_4x, 3);

            uint8* TexelPtr0 = TextureMemory + Fetch0;
            uint8* TexelPtr1 = TextureMemory + Fetch1;
            uint8* TexelPtr2 = TextureMemory + Fetch2;
            uint8* TexelPtr3 = TextureMemory + Fetch3;

            __m128i SampleA = _mm_setr_epi32(
                *(uint32*)TexelPtr0,
                *(uint32*)TexelPtr1,
                *(uint32*)TexelPtr2,
                *(uint32*)TexelPtr3
            );
            __m128i SampleB = _mm_setr_epi32(
                *(uint32*)(TexelPtr0 + BITMAP_BYTES_PER_PIXEL),
                *(uint32*)(TexelPtr1 + BITMAP_BYTES_PER_PIXEL),
                *(uint32*)(TexelPtr2 + BITMAP_BYTES_PER_PIXEL),
                *(uint32*)(TexelPtr3 + BITMAP_BYTES_PER_PIXEL)
            );
            __m128i SampleC = _mm_setr_epi32(
                *(uint32*)(TexelPtr0 + TexturePitch),
                *(uint32*)(TexelPtr1 + TexturePitch),
                *(uint32*)(TexelPtr2 + TexturePitch),
                *(uint32*)(TexelPtr3 + TexturePitch)
            );
            __m128i SampleD = _mm_setr_epi32(
                *(uint32*)(TexelPtr0 + TexturePitch + BITMAP_BYTES_PER_PIXEL),
                *(uint32*)(TexelPtr1 + TexturePitch + BITMAP_BYTES_PER_PIXEL),
                *(uint32*)(TexelPtr2 + TexturePitch + BITMAP_BYTES_PER_PIXEL),
                *(uint32*)(TexelPtr3 + TexturePitch + BITMAP_BYTES_PER_PIXEL)
            );
#else
            SampleA = TextureXFloored;
            SampleB = TextureXFloored;
            SampleC = TextureXFloored;
            SampleD = TextureXFloored;
#endif

#if 1
            __m128i TexelArb = _mm_and_si128(SampleA, MaskFF00FF_4x);
            __m128i TexelAag = _mm_and_si128(SampleA, MaskFF00FF00_4x);
            TexelArb = _mm_mullo_epi16(TexelArb, TexelArb);
            __m128 TexelAa = _mm_cvtepi32_ps(_mm_srli_epi32(TexelAag, 24));
            TexelAag = _mm_mulhi_epu16(TexelAag, TexelAag);

            __m128i TexelBrb = _mm_and_si128(SampleB, MaskFF00FF_4x);
            __m128i TexelBag = _mm_and_si128(SampleB, MaskFF00FF00_4x);
            TexelBrb = _mm_mullo_epi16(TexelBrb, TexelBrb);
            __m128 TexelBa = _mm_cvtepi32_ps(_mm_srli_epi32(TexelBag, 24));
            TexelBag = _mm_mulhi_epu16(TexelBag, TexelBag);

            __m128i TexelCrb = _mm_and_si128(SampleC, MaskFF00FF_4x);
            __m128i TexelCag = _mm_and_si128(SampleC, MaskFF00FF00_4x);
            TexelCrb = _mm_mullo_epi16(TexelCrb, TexelCrb);
            __m128 TexelCa = _mm_cvtepi32_ps(_mm_srli_epi32(TexelCag, 24));
            TexelCag = _mm_mulhi_epu16(TexelCag, TexelCag);

            __m128i TexelDrb = _mm_and_si128(SampleD, MaskFF00FF_4x);
            __m128i TexelDag = _mm_and_si128(SampleD, MaskFF00FF00_4x);
            TexelDrb = _mm_mullo_epi16(TexelDrb, TexelDrb);
            __m128 TexelDa = _mm_cvtepi32_ps(_mm_srli_epi32(TexelDag, 24));
            TexelDag = _mm_mulhi_epu16(TexelDag, TexelDag);

            __m128 TexelAr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelArb, 16));
            __m128 TexelAg = _mm_cvtepi32_ps(_mm_and_si128(TexelAag, MaskFFFF_4x));
            __m128 TexelAb = _mm_cvtepi32_ps(_mm_and_si128(TexelArb, MaskFFFF_4x));

            __m128 TexelBr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelBrb, 16));
            __m128 TexelBg = _mm_cvtepi32_ps(_mm_and_si128(TexelBag, MaskFFFF_4x));
            __m128 TexelBb = _mm_cvtepi32_ps(_mm_and_si128(TexelBrb, MaskFFFF_4x));

            __m128 TexelCr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelCrb, 16));
            __m128 TexelCg = _mm_cvtepi32_ps(_mm_and_si128(TexelCag, MaskFFFF_4x));
            __m128 TexelCb = _mm_cvtepi32_ps(_mm_and_si128(TexelCrb, MaskFFFF_4x));

            __m128 TexelDr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelDrb, 16));
            __m128 TexelDg = _mm_cvtepi32_ps(_mm_and_si128(TexelDag, MaskFFFF_4x));
            __m128 TexelDb = _mm_cvtepi32_ps(_mm_and_si128(TexelDrb, MaskFFFF_4x));
#else
            __m128 TexelAr = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleA, 16), MaskFF_4x));
            __m128 TexelAg = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleA, 8), MaskFF_4x));
            __m128 TexelAb = _mm_cvtepi32_ps(_mm_and_si128(SampleA, MaskFF_4x));
            __m128 TexelAa = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleA, 24), MaskFF_4x));

            __m128 TexelBr = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleB, 16), MaskFF_4x));
            __m128 TexelBg = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleB, 8), MaskFF_4x));
            __m128 TexelBb = _mm_cvtepi32_ps(_mm_and_si128(SampleB, MaskFF_4x));
            __m128 TexelBa = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleB, 24), MaskFF_4x));

            __m128 TexelCr = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleC, 16), MaskFF_4x));
            __m128 TexelCg = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleC, 8), MaskFF_4x));
            __m128 TexelCb = _mm_cvtepi32_ps(_mm_and_si128(SampleC, MaskFF_4x));
            __m128 TexelCa = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleC, 24), MaskFF_4x));

            __m128 TexelDr = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleD, 16), MaskFF_4x));
            __m128 TexelDg = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleD, 8), MaskFF_4x));
            __m128 TexelDb = _mm_cvtepi32_ps(_mm_and_si128(SampleD, MaskFF_4x));
            __m128 TexelDa = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleD, 24), MaskFF_4x));

            TexelAr = mmSquare(TexelAr);
            TexelAg = mmSquare(TexelAg);
            TexelAb = mmSquare(TexelAb);
            TexelBr = mmSquare(TexelBr);
            TexelBg = mmSquare(TexelBg);
            TexelBb = mmSquare(TexelBb);
            TexelCr = mmSquare(TexelCr);
            TexelCg = mmSquare(TexelCg);
            TexelCb = mmSquare(TexelCb);
            TexelDr = mmSquare(TexelDr);
            TexelDg = mmSquare(TexelDg);
            TexelDb = mmSquare(TexelDb);
#endif

            __m128 Destr = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 16), MaskFF_4x));
            __m128 Destg = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 8), MaskFF_4x));
            __m128 Destb = _mm_cvtepi32_ps(_mm_and_si128(OriginalDest, MaskFF_4x));
            __m128 Desta = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 24), MaskFF_4x));

            __m128 ifx = _mm_sub_ps(One_4x, TextureXf);
            __m128 ify = _mm_sub_ps(One_4x, TextureYf);

            __m128 l0 = _mm_mul_ps(ify, ifx);
            __m128 l1 = _mm_mul_ps(ify, TextureXf);
            __m128 l2 = _mm_mul_ps(TextureYf, ifx);
            __m128 l3 = _mm_mul_ps(TextureYf, TextureXf);

            __m128 Texelr = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAr), _mm_mul_ps(l1, TexelBr)), _mm_add_ps(_mm_mul_ps(l2, TexelCr), _mm_mul_ps(l3, TexelDr)));
            __m128 Texelg = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAg), _mm_mul_ps(l1, TexelBg)), _mm_add_ps(_mm_mul_ps(l2, TexelCg), _mm_mul_ps(l3, TexelDg)));
            __m128 Texelb = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAb), _mm_mul_ps(l1, TexelBb)), _mm_add_ps(_mm_mul_ps(l2, TexelCb), _mm_mul_ps(l3, TexelDb)));
            __m128 Texela = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAa), _mm_mul_ps(l1, TexelBa)), _mm_add_ps(_mm_mul_ps(l2, TexelCa), _mm_mul_ps(l3, TexelDa)));

            Texelr = _mm_mul_ps(Texelr, Colorr_4x);
            Texelg = _mm_mul_ps(Texelg, Colorg_4x);
            Texelb = _mm_mul_ps(Texelb, Colorb_4x);
            Texela = _mm_mul_ps(Texela, Colora_4x);

            Texelr = _mm_min_ps(_mm_max_ps(Texelr, Zero_4x), MaxColorValue_4x);
            Texelg = _mm_min_ps(_mm_max_ps(Texelg, Zero_4x), MaxColorValue_4x);
            Texelb = _mm_min_ps(_mm_max_ps(Texelb, Zero_4x), MaxColorValue_4x);

            Destr = mmSquare(Destr);
            Destg = mmSquare(Destg);
            Destb = mmSquare(Destb);

            __m128 InvTexelA = _mm_sub_ps(One_4x, _mm_mul_ps(Inv255_4x, Texela));
            __m128 Blendedr = _mm_add_ps(_mm_mul_ps(InvTexelA, Destr), Texelr);
            __m128 Blendedg = _mm_add_ps(_mm_mul_ps(InvTexelA, Destg), Texelg);
            __m128 Blendedb = _mm_add_ps(_mm_mul_ps(InvTexelA, Destb), Texelb);
            __m128 Blendeda = _mm_add_ps(_mm_mul_ps(InvTexelA, Desta), Texela);

            Blendedr = _mm_mul_ps(_mm_rsqrt_ps(Blendedr), Blendedr);
            Blendedg = _mm_mul_ps(_mm_rsqrt_ps(Blendedg), Blendedg);
            Blendedb = _mm_mul_ps(_mm_rsqrt_ps(Blendedb), Blendedb);

            __m128i Intr = _mm_cvtps_epi32(Blendedr);
            __m128i Intg = _mm_cvtps_epi32(Blendedg);
            __m128i Intb = _mm_cvtps_epi32(Blendedb);
            __m128i Inta = _mm_cvtps_epi32(Blendeda);

            __m128i Out = _mm_or_si128(
                _mm_or_si128(_mm_slli_epi32(Intr, 16), _mm_slli_epi32(Intg, 8)),
                _mm_or_si128(Intb, _mm_slli_epi32(Inta, 24))
            );
#else
            __m128i Out = _mm_or_si128(TextureXFloored, TextureYFloored);
#endif
            __m128i MaskedOut = _mm_or_si128(
                _mm_and_si128(WriteMask, Out),
                _mm_andnot_si128(WriteMask, OriginalDest)
            );

            _mm_store_si128((__m128i*)Pixel, MaskedOut);

            Pixel += 4;
            ClipMask = _mm_set1_epi32(0xFFFFFFFF);

            if (XI + 8 > MaxX) {
                ClipMask = EndClipMask;
            }

            IACA_VC64_END;
        }
        Row += RowAdvance;
    }
    END_TIMED_BLOCK_COUNTED(ProcessPixel, GetClampedRectArea(FillRect) / 2);
    END_TIMED_BLOCK(DrawRectangleQuickly);
}

internal void DrawBitmap(
    loaded_bitmap* Buffer, loaded_bitmap* BMP,
    real32 RealStartX, real32 RealStartY,
    real32 CAlpha = 1.0f
) {

    int32 StartX = RoundReal32ToInt32(RealStartX);
    int32 StartY = RoundReal32ToInt32(RealStartY);

    int32 EndX = StartX + BMP->Width;
    int32 EndY = StartY + BMP->Height;

    int32 SourceOffsetX = 0;
    if (StartX < 0) {
        SourceOffsetX = -StartX;
        StartX = 0;
    }

    int32 SourceOffsetY = 0;
    if (StartY < 0) {
        SourceOffsetY = -StartY;
        StartY = 0;
    }

    if (EndX > Buffer->Width) {
        EndX = Buffer->Width;
    }
    if (EndY > Buffer->Height) {
        EndY = Buffer->Height;
    }

    uint8* SourceRow =
        (uint8*)BMP->Memory + SourceOffsetY * BMP->Pitch + SourceOffsetX * BITMAP_BYTES_PER_PIXEL;
    uint8* DestRow =
        (uint8*)Buffer->Memory + StartY * Buffer->Pitch + StartX * BITMAP_BYTES_PER_PIXEL;

    for (int32 Y = StartY; Y < EndY; ++Y) {
        uint32* Dest = (uint32*)DestRow;
        uint32* Source = (uint32*)SourceRow;
        for (int32 X = StartX; X < EndX; ++X) {

            v4 Texel = V4(
                (real32)((*Source >> 16) & 0xFF),
                (real32)((*Source >> 8) & 0xFF),
                (real32)((*Source) & 0xFF),
                (real32)((*Source >> 24))
            );

            Texel = SRGB255ToLinear1(Texel);
            Texel *= CAlpha;

            v4 Destv4 = V4(
                (real32)((*Dest >> 16) & 0xFF),
                (real32)((*Dest >> 8) & 0xFF),
                (real32)((*Dest) & 0xFF),
                (real32)((*Dest >> 24) & 0xFF)
            );
            Destv4 = SRGB255ToLinear1(Destv4);

            v4 Result = (1.0f - Texel.a) * Destv4 + Texel;
            Result = Linear1ToSRGB255(Result);

            *Dest =
                (RoundReal32ToUint32(Result.a) << 24) |
                (RoundReal32ToUint32(Result.r) << 16) |
                (RoundReal32ToUint32(Result.g) << 8) |
                (RoundReal32ToUint32(Result.b));

            ++Dest;
            ++Source;
        }
        DestRow += Buffer->Pitch;
        SourceRow += BMP->Pitch;
    }
}

internal void ChangeSaturation(loaded_bitmap* Buffer, real32 Level) {

    uint8* DestRow = (uint8*)Buffer->Memory;

    for (int32 Y = 0; Y < Buffer->Height; ++Y) {
        uint32* Dest = (uint32*)DestRow;

        for (int32 X = 0; X < Buffer->Width; ++X) {

            v4 Destv4 = V4(
                (real32)((*Dest >> 16) & 0xFF),
                (real32)((*Dest >> 8) & 0xFF),
                (real32)((*Dest) & 0xFF),
                (real32)((*Dest >> 24) & 0xFF)
            );
            Destv4 = SRGB255ToLinear1(Destv4);

            real32 Avg = (Destv4.r + Destv4.g + Destv4.b) / 3.0f;
            v3 Avg3 = V3(Avg, Avg, Avg);
            v3 Delta = Destv4.rgb - Avg3;
            Delta *= Level;

            v4 Result = V4(Delta + Avg3, Destv4.a);
            Result = Linear1ToSRGB255(Result);

            *Dest =
                (RoundReal32ToUint32(Result.a) << 24) |
                (RoundReal32ToUint32(Result.r) << 16) |
                (RoundReal32ToUint32(Result.g) << 8) |
                (RoundReal32ToUint32(Result.b));

            ++Dest;
        }
        DestRow += Buffer->Pitch;
    }
}

internal void DrawMatte(
    loaded_bitmap* Buffer, loaded_bitmap* BMP,
    real32 RealStartX, real32 RealStartY,
    real32 CAlpha = 1.0f
) {

    int32 StartX = RoundReal32ToInt32(RealStartX);
    int32 StartY = RoundReal32ToInt32(RealStartY);

    int32 EndX = StartX + BMP->Width;
    int32 EndY = StartY + BMP->Height;

    int32 SourceOffsetX = 0;
    if (StartX < 0) {
        SourceOffsetX = -StartX;
        StartX = 0;
    }

    int32 SourceOffsetY = 0;
    if (StartY < 0) {
        SourceOffsetY = -StartY;
        StartY = 0;
    }

    if (EndX > Buffer->Width) {
        EndX = Buffer->Width;
    }
    if (EndY > Buffer->Height) {
        EndY = Buffer->Height;
    }

    uint8* SourceRow =
        (uint8*)BMP->Memory + SourceOffsetY * BMP->Pitch + SourceOffsetX * BITMAP_BYTES_PER_PIXEL;
    uint8* DestRow =
        (uint8*)Buffer->Memory + StartY * Buffer->Pitch + StartX * BITMAP_BYTES_PER_PIXEL;

    for (int32 Y = StartY; Y < EndY; ++Y) {
        uint32* Dest = (uint32*)DestRow;
        uint32* Source = (uint32*)SourceRow;
        for (int32 X = StartX; X < EndX; ++X) {

            real32 SA = CAlpha * (real32)((*Source >> 24));
            real32 SR = CAlpha * (real32)((*Source >> 16) & 0xFF);
            real32 SG = CAlpha * (real32)((*Source >> 8) & 0xFF);
            real32 SB = CAlpha * (real32)((*Source) & 0xFF);

            real32 DA = (real32)((*Dest >> 24) & 0xFF);
            real32 DR = (real32)((*Dest >> 16) & 0xFF);
            real32 DG = (real32)((*Dest >> 8) & 0xFF);
            real32 DB = (real32)((*Dest) & 0xFF);

            real32 SA01 = SA / 255.0f;
            real32 DA01 = DA / 255.0f;
            real32 InvSA01 = 1.0f - SA01;
            real32 RA = InvSA01 * DA;
            real32 RR = InvSA01 * DR;
            real32 RG = InvSA01 * DG;
            real32 RB = InvSA01 * DB;

            *Dest = (RoundReal32ToUint32(RA) << 24) | (RoundReal32ToUint32(RR) << 16) |
                (RoundReal32ToUint32(RG) << 8) | (RoundReal32ToUint32(RB));

            ++Dest;
            ++Source;
        }
        DestRow += Buffer->Pitch;
        SourceRow += BMP->Pitch;
    }
}

internal void RenderGroupToOutput(
    render_group* RenderGroup, loaded_bitmap* OutputTarget,
    rectangle2i ClipRect, bool32 Even
) {
    BEGIN_TIMED_BLOCK(RenderGroupToOutput);
    v2 ScreenDim = V2i(OutputTarget->Width, OutputTarget->Height);
    real32 PixelsToMeters = 1.0f / RenderGroup->MetersToPixels;

    for (uint32 BaseAddress = 0; BaseAddress < RenderGroup->PushBufferSize;) {

        render_group_entry_header* Header =
            (render_group_entry_header*)(RenderGroup->PushBufferBase + BaseAddress);
        void* Data = (uint8*)Header + sizeof(render_group_entry_header);
        BaseAddress += sizeof(render_group_entry_header);

        switch (Header->Type) {
        case RenderGroupEntryType_render_entry_clear: {
            render_entry_clear* Entry = (render_entry_clear*)Data;

            DrawRectangle(
                OutputTarget, V2(0, 0), V2i(OutputTarget->Width, OutputTarget->Height),
                Entry->Color, ClipRect, Even
            );

            BaseAddress += sizeof(*Entry);
        } break;

        case RenderGroupEntryType_render_entry_saturation: {
            render_entry_saturation* Entry = (render_entry_saturation*)Data;

            ChangeSaturation(OutputTarget, Entry->Level);

            BaseAddress += sizeof(*Entry);
        } break;

        case RenderGroupEntryType_render_entry_bitmap: {
            render_entry_bitmap* Entry = (render_entry_bitmap*)Data;
            entity_basis_p_result Basis = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis, ScreenDim);
            Assert(Entry->Bitmap);
#if 0
            DrawRectangleSlowly(
                OutputTarget,
                Basis.P,
                Basis.Scale * V2(Entry->Size.x, 0),
                Basis.Scale * V2(0, Entry->Size.y),
                Entry->Color,
                Entry->Bitmap, 0, 0, 0, 0, PixelsToMeters
            );
#else
            DrawRectangleQuickly(
                OutputTarget,
                Basis.P,
                Basis.Scale * V2(Entry->Size.x, 0),
                Basis.Scale * V2(0, Entry->Size.y),
                Entry->Color,
                Entry->Bitmap, PixelsToMeters,
                ClipRect,
                Even
            );
#endif
            BaseAddress += sizeof(*Entry);
        } break;

        case RenderGroupEntryType_render_entry_rectangle: {
            render_entry_rectangle* Entry = (render_entry_rectangle*)Data;
            entity_basis_p_result Basis = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis, ScreenDim);
            DrawRectangle(OutputTarget, Basis.P, Basis.P + Entry->Dim * Basis.Scale, Entry->Color, ClipRect, Even);
            BaseAddress += sizeof(*Entry);
        } break;

        case RenderGroupEntryType_render_entry_coordinate_system: {
            render_entry_coordinate_system* Entry = (render_entry_coordinate_system*)Data;

            v2 P = Entry->Origin;
            v2 Dim = V2(2, 2);

            v2 PX = P + Entry->XAxis;
            v2 PY = P + Entry->YAxis;

            v2 PMax = P + Entry->XAxis + Entry->YAxis;

            DrawRectangle(OutputTarget, P - Dim, P + Dim, Entry->Color, ClipRect, Even);
            DrawRectangle(OutputTarget, PX - Dim, PX + Dim, Entry->Color, ClipRect, Even);
            DrawRectangle(OutputTarget, PY - Dim, PY + Dim, Entry->Color, ClipRect, Even);
            DrawRectangle(OutputTarget, PMax - Dim, PMax + Dim, Entry->Color, ClipRect, Even);
            DrawRectangleSlowly(
                OutputTarget, Entry->Origin, Entry->XAxis, Entry->YAxis, Entry->Color,
                Entry->Texture, Entry->NormalMap,
                Entry->Top, Entry->Middle, Entry->Bottom, PixelsToMeters
            );

#if 0
            for (uint32 PIndex = 0; PIndex < ArrayCount(Entry->Points); PIndex++) {
                v2 Point = Entry->Points[PIndex];
                v2 PPoint = Entry->Origin + Point.x * Entry->XAxis + Point.y * Entry->YAxis;
                DrawRectangle(OutputTarget, PPoint - Dim, PPoint + Dim, Entry->Color.r, Entry->Color.g, Entry->Color.b);
            }
#endif
            BaseAddress += sizeof(*Entry);
        } break;
            InvalidDefaultCase;
        }
    }
    END_TIMED_BLOCK(RenderGroupToOutput);
}

struct tile_render_work {
    render_group* RenderGroup;
    loaded_bitmap* OutputTarget;
    rectangle2i ClipRect;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(DoTiledRenderWork) {
    tile_render_work* Work = (tile_render_work*)Data;
    RenderGroupToOutput(Work->RenderGroup, Work->OutputTarget, Work->ClipRect, true);
    RenderGroupToOutput(Work->RenderGroup, Work->OutputTarget, Work->ClipRect, false);
}

internal void TiledRenderGroupToOutput(
    platform_work_queue* RenderQueue,
    render_group* RenderGroup, loaded_bitmap* OutputTarget
) {
    int32 const TileCountX = 4;
    int32 const TileCountY = 4;
    tile_render_work WorkArray[TileCountX * TileCountY];
    Assert(((uintptr)OutputTarget->Memory & 15) == 0);
    int32 TileWidth = OutputTarget->Width / TileCountX;
    int32 TileHeight = OutputTarget->Height / TileCountY;
    TileWidth = ((TileWidth + 3) / 4) * 4;
    int32 WorkCount = 0;
    for (int32 TileY = 0; TileY < TileCountY; ++TileY) {
        for (int32 TileX = 0; TileX < TileCountX; ++TileX) {
            rectangle2i ClipRect;
            ClipRect.MinX = TileX * TileWidth;
            ClipRect.MaxX = ClipRect.MinX + TileWidth;
            if (TileX == TileCountX - 1) {
                ClipRect.MaxX = OutputTarget->Width;
            }
            ClipRect.MinY = TileY * TileHeight;
            ClipRect.MaxY = ClipRect.MinY + TileHeight;
            if (TileY == TileCountY - 1) {
                ClipRect.MaxY = OutputTarget->Height;
            }
            tile_render_work* Work = WorkArray + WorkCount++;
            Work->ClipRect = ClipRect;
            Work->OutputTarget = OutputTarget;
            Work->RenderGroup = RenderGroup;
#if 1
            PlatformAddEntry(RenderQueue, DoTiledRenderWork, Work);
#else
            DoTiledRenderWork(RenderQueue, Work);
#endif
        }
    }
    PlatformCompleteAllWork(RenderQueue);
}
