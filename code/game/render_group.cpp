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

    //* Top and bottom
    PushRect(Group, Offset - V3(0, 0.5f * Dim.y, 0), V2(Dim.x, Thickness), Color);
    PushRect(Group, Offset + V3(0, 0.5f * Dim.y, 0), V2(Dim.x, Thickness), Color);

    //* Left and right
    PushRect(Group, Offset - V3(0.5f * Dim.x, 0, 0), V2(Thickness, Dim.y), Color);
    PushRect(Group, Offset + V3(0.5f * Dim.x, 0, 0), V2(Thickness, Dim.y), Color);
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
    v4 Color
) {
    int32 MinX = RoundReal32ToInt32(vMin.x);
    int32 MinY = RoundReal32ToInt32(vMin.y);
    int32 MaxX = RoundReal32ToInt32(vMax.x);
    int32 MaxY = RoundReal32ToInt32(vMax.y);

    if (MinX < 0) {
        MinX = 0;
    }
    if (MinY < 0) {
        MinY = 0;
    }

    if (MaxX > Buffer->Width) {
        MaxX = Buffer->Width;
    }
    if (MaxY > Buffer->Height) {
        MaxY = Buffer->Height;
    }

    uint32 Color32 =
        (RoundReal32ToUint32(Color.a * 255.0f) << 24) |
        (RoundReal32ToUint32(Color.r * 255.0f) << 16) |
        (RoundReal32ToUint32(Color.g * 255.0f) << 8) |
        (RoundReal32ToUint32(Color.b * 255.0f));

    uint8* Row = (uint8*)Buffer->Memory + MinY * Buffer->Pitch + MinX * BITMAP_BYTES_PER_PIXEL;
    for (int32 Y = MinY; Y < MaxY; ++Y) {
        uint32* Pixel = (uint32*)Row;
        for (int32 X = MinX; X < MaxX; ++X) {
            *Pixel++ = Color32;
        }
        Row += Buffer->Pitch;
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

internal void DrawRectangleSlowly(
    loaded_bitmap* Buffer,
    v2 Origin, v2 XAxis, v2 YAxis,
    v4 Color, loaded_bitmap* Texture, loaded_bitmap* NormalMap,
    environment_map* Top, environment_map* Middle, environment_map* Bottom,
    real32 PixelsToMeters
) {
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
}

internal void
DrawRectangleOutline(
    loaded_bitmap* Buffer,
    v2 vMin, v2 vMax,
    v3 Color,
    real32 T = 2.0f
) {
    DrawRectangle(Buffer, V2(vMin.x - T, vMax.y - T), V2(vMax.x + T, vMax.y + T), V4(Color, 1.0f));
    DrawRectangle(Buffer, V2(vMin.x - T, vMin.y - T), V2(vMax.x + T, vMin.y + T), V4(Color, 1.0f));
    DrawRectangle(Buffer, V2(vMin.x - T, vMin.y - T), V2(vMin.x + T, vMax.y + T), V4(Color, 1.0f));
    DrawRectangle(Buffer, V2(vMax.x - T, vMin.y - T), V2(vMax.x + T, vMax.y + T), V4(Color, 1.0f));
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

internal void RenderGroupToOutput(render_group* RenderGroup, loaded_bitmap* OutputTarget) {

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
                Entry->Color
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
            DrawBitmap(OutputTarget, Entry->Bitmap, Basis.x, Basis.y, Entry->Color.a);
#else
            DrawRectangleSlowly(
                OutputTarget,
                Basis.P,
                Basis.Scale * V2(Entry->Size.x, 0),
                Basis.Scale * V2(0, Entry->Size.y),
                Entry->Color,
                Entry->Bitmap, 0,
                0, 0, 0, PixelsToMeters
            );
#endif
            BaseAddress += sizeof(*Entry);
        } break;

        case RenderGroupEntryType_render_entry_rectangle: {
            render_entry_rectangle* Entry = (render_entry_rectangle*)Data;
            entity_basis_p_result Basis = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis, ScreenDim);
            DrawRectangle(OutputTarget, Basis.P, Basis.P + Entry->Dim * Basis.Scale, Entry->Color);
            BaseAddress += sizeof(*Entry);
        } break;

        case RenderGroupEntryType_render_entry_coordinate_system: {
            render_entry_coordinate_system* Entry = (render_entry_coordinate_system*)Data;

            v2 P = Entry->Origin;
            v2 Dim = V2(2, 2);

            v2 PX = P + Entry->XAxis;
            v2 PY = P + Entry->YAxis;

            v2 PMax = P + Entry->XAxis + Entry->YAxis;

            DrawRectangle(OutputTarget, P - Dim, P + Dim, Entry->Color);
            DrawRectangle(OutputTarget, PX - Dim, PX + Dim, Entry->Color);
            DrawRectangle(OutputTarget, PY - Dim, PY + Dim, Entry->Color);
            DrawRectangle(OutputTarget, PMax - Dim, PMax + Dim, Entry->Color);
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
}
