#include "math.cpp"
#include "bmp.cpp"
#include "sim_region.cpp"

struct render_basis {
    v3 P;
};

struct render_entity_basis {
    render_basis* Basis;
    v2 Offset;
    real32 OffsetZ;
    real32 EntityZC;
};

enum render_group_entry_type {
    RenderGroupEntryType_render_entry_clear,
    RenderGroupEntryType_render_entry_bitmap,
    RenderGroupEntryType_render_entry_rectangle,
    RenderGroupEntryType_render_entry_coordinate_system
};

struct render_group_entry_header {
    render_group_entry_type Type;
};

struct render_entry_clear {
    v4 Color;
};

struct render_entry_coordinate_system {
    v2 Origin;
    v2 XAxis;
    v2 YAxis;
    v4 Color;
    loaded_bitmap* Texture;
    v2 Points[16];
};

struct render_entry_bitmap {
    loaded_bitmap* Bitmap;
    render_entity_basis EntityBasis;
    real32 R, G, B, A;
};

struct render_entry_rectangle {
    render_entity_basis EntityBasis;
    v2 Dim;
    real32 R, G, B, A;
};

struct render_group {
    render_basis* DefaultBasis;
    real32 MetersToPixels;
    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8* PushBufferBase;
};

internal render_group*
AllocateRenderGroup(memory_arena* Arena, uint32 MaxPushBufferSize, real32 MetersToPixels) {
    render_group* Result = PushStruct(Arena, render_group);
    Result->PushBufferBase = (uint8*)PushSize(Arena, MaxPushBufferSize);
    Result->DefaultBasis = PushStruct(Arena, render_basis);
    Result->DefaultBasis->P = V3(0, 0, 0);
    Result->MetersToPixels = MetersToPixels;
    Result->MaxPushBufferSize = MaxPushBufferSize;
    Result->PushBufferSize = 0;
    return Result;
}

internal v2 GetRenderEntityBasisP(
    render_group* RenderGroup, render_entity_basis* EntityBasis, v2 ScreenCenter
) {
    v3 EntityBaseP = EntityBasis->Basis->P;
    real32 ZFudge = 1.0f + 0.1f * (EntityBaseP.z + EntityBasis->OffsetZ);
    real32 EntityGroudX = ScreenCenter.x + EntityBaseP.x * RenderGroup->MetersToPixels * ZFudge;
    real32 EntityGroudY = ScreenCenter.y - EntityBaseP.y * RenderGroup->MetersToPixels * ZFudge;
    real32 EntityZ = -EntityBaseP.z * RenderGroup->MetersToPixels;
    v2 Center = V2(
        EntityGroudX + EntityBasis->Offset.x,
        EntityGroudY + EntityBasis->Offset.y + EntityZ * EntityBasis->EntityZC
    );
    return Center;
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

internal inline void PushPiece(
    render_group* Group, loaded_bitmap* Bitmap,
    v2 Offset, real32 OffsetZ, v2 Align, v2 Dim,
    v4 Color,
    real32 EntityZC = 1.0f
) {
    real32 MetersToPixels = Group->MetersToPixels;
    v2 OffsetFixY = { Offset.x, -Offset.y };
    render_entry_bitmap* Piece = PushRenderElement(Group, render_entry_bitmap);
    if (Piece) {
        Piece->EntityBasis.Basis = Group->DefaultBasis;
        Piece->R = Color.r;
        Piece->G = Color.g;
        Piece->B = Color.b;
        Piece->A = Color.a;
        Piece->Bitmap = Bitmap;
        Piece->EntityBasis.Offset = MetersToPixels * OffsetFixY - Align;
        Piece->EntityBasis.OffsetZ = OffsetZ;
        Piece->EntityBasis.EntityZC = EntityZC;
    }
}

internal inline void
PushBitmap(
    render_group* Group, loaded_bitmap* Bitmap,
    v2 Offset, real32 OffsetZ, v2 Align, real32 Alpha = 1.0f,
    real32 EntityZC = 1.0f
) {
    PushPiece(
        Group, Bitmap, Offset, OffsetZ, Align, { 0, 0 }, { 0, 0, 0, Alpha }, EntityZC
    );
}

internal inline void PushRect(
    render_group* Group,
    v2 Offset, real32 OffsetZ, v2 Dim,
    v4 Color,
    real32 EntityZC = 1.0f
) {
    real32 MetersToPixels = Group->MetersToPixels;
    v2 OffsetFixY = { Offset.x, -Offset.y };
    render_entry_rectangle* Piece = PushRenderElement(Group, render_entry_rectangle);
    if (Piece) {
        v2 HalfDim = Group->MetersToPixels * Dim * 0.5f;
        Piece->EntityBasis.Basis = Group->DefaultBasis;
        Piece->R = Color.r;
        Piece->G = Color.g;
        Piece->B = Color.b;
        Piece->A = Color.a;
        Piece->EntityBasis.Offset = MetersToPixels * OffsetFixY - V2(HalfDim.x, HalfDim.y);
        Piece->EntityBasis.OffsetZ = OffsetZ;
        Piece->EntityBasis.EntityZC = EntityZC;
        Piece->Dim = Group->MetersToPixels * Dim;
    }
}

internal void Clear(render_group* Group, v4 Color) {
    render_entry_clear* Entry = PushRenderElement(Group, render_entry_clear);
    if (Entry) {
        Entry->Color = Color;
    }
}

internal void PushRectOutline(
    render_group* Group,
    v2 Offset, real32 OffsetZ, v2 Dim,
    v4 Color,
    real32 EntityZC = 1.0f
) {
    real32 Thickness = 0.1f;

    //* Top and bottom
    PushRect(Group, Offset - V2(0, 0.5f * Dim.y), OffsetZ, V2(Dim.x, Thickness), Color, EntityZC);
    PushRect(Group, Offset + V2(0, 0.5f * Dim.y), OffsetZ, V2(Dim.x, Thickness), Color, EntityZC);

    //* Left and right
    PushRect(Group, Offset - V2(0.5f * Dim.x, 0), OffsetZ, V2(Thickness, Dim.y), Color, EntityZC);
    PushRect(Group, Offset + V2(0.5f * Dim.x, 0), OffsetZ, V2(Thickness, Dim.y), Color, EntityZC);
}

internal void CoordinateSystem(
    render_group* Group, v2 Origin, v2 XAxis, v2 YAxis, v4 Color, loaded_bitmap* Texture
) {
    render_entry_coordinate_system* Entry = PushRenderElement(Group, render_entry_coordinate_system);
    if (Entry) {
        Entry->Origin = Origin;
        Entry->XAxis = XAxis;
        Entry->YAxis = YAxis;
        Entry->Color = Color;
        Entry->Texture = Texture;
        uint32 PIndex = 0;
        for (real32 Y = 0.0f; Y < 1.0f; Y += 0.25f) {
            for (real32 X = 0.0f; X < 1.0f; X += 0.25f) {
                Entry->Points[PIndex++] = V2(X, Y);
            }
        }
    }
}

/// Maximums are not inclusive
internal void DrawRectangle(
    loaded_bitmap* Buffer,
    v2 vMin, v2 vMax,
    real32 R, real32 G, real32 B, real32 A = 1.0f
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

    uint32 Color =
        (RoundReal32ToUint32(A * 255.0f) << 24) |
        (RoundReal32ToUint32(R * 255.0f) << 16) |
        (RoundReal32ToUint32(G * 255.0f) << 8) |
        (RoundReal32ToUint32(B * 255.0f));

    uint8* Row = (uint8*)Buffer->Memory + MinY * Buffer->Pitch + MinX * BITMAP_BYTES_PER_PIXEL;
    for (int32 Y = MinY; Y < MaxY; ++Y) {
        uint32* Pixel = (uint32*)Row;
        for (int32 X = MinX; X < MaxX; ++X) {
            *Pixel++ = Color;
        }
        Row += Buffer->Pitch;
    }
}

internal void DrawRectangleSlowly(
    loaded_bitmap* Buffer,
    v2 Origin, v2 XAxis, v2 YAxis,
    v4 Color, loaded_bitmap* Texture
) {
    Color.rgb *= Color.a;

    int32 WidthMax = Buffer->Width - 1;
    int32 HeightMax = Buffer->Height - 1;
    int32 XMin = WidthMax;
    int32 XMax = 0;
    int32 YMin = HeightMax;
    int32 YMax = 0;

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

                uint8* TexelPtr = (uint8*)Texture->Memory + TextureYFloored * Texture->Pitch + TextureXFloored * BITMAP_BYTES_PER_PIXEL;
                uint32 TexelA = *(uint32*)TexelPtr;
                uint32 TexelB = *(uint32*)(TexelPtr + BITMAP_BYTES_PER_PIXEL);
                uint32 TexelC = *(uint32*)(TexelPtr + Texture->Pitch);
                uint32 TexelD = *(uint32*)(TexelPtr + Texture->Pitch + BITMAP_BYTES_PER_PIXEL);

                v4 TexelAv4 = V4(
                    (real32)((TexelA >> 16) & 0xFF),
                    (real32)((TexelA >> 8) & 0xFF),
                    (real32)((TexelA) & 0xFF),
                    (real32)((TexelA >> 24))
                );
                v4 TexelBv4 = V4(
                    (real32)((TexelB >> 16) & 0xFF),
                    (real32)((TexelB >> 8) & 0xFF),
                    (real32)((TexelB) & 0xFF),
                    (real32)((TexelB >> 24))
                );
                v4 TexelCv4 = V4(
                    (real32)((TexelC >> 16) & 0xFF),
                    (real32)((TexelC >> 8) & 0xFF),
                    (real32)((TexelC) & 0xFF),
                    (real32)((TexelC >> 24))
                );
                v4 TexelDv4 = V4(
                    (real32)((TexelD >> 16) & 0xFF),
                    (real32)((TexelD >> 8) & 0xFF),
                    (real32)((TexelD) & 0xFF),
                    (real32)((TexelD >> 24))
                );

                v4 TexelA01 = SRGB255ToLinear1(TexelAv4);
                v4 TexelB01 = SRGB255ToLinear1(TexelBv4);
                v4 TexelC01 = SRGB255ToLinear1(TexelCv4);
                v4 TexelD01 = SRGB255ToLinear1(TexelDv4);

                v4 Texel = Lerp(
                    Lerp(TexelA01, TextureXf, TexelB01),
                    TextureYf,
                    Lerp(TexelC01, TextureXf, TexelD01)
                );
                Texel = Hadamard(Texel, Color);

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
    DrawRectangle(Buffer, V2(vMin.x - T, vMax.y - T), V2(vMax.x + T, vMax.y + T), Color.r, Color.g, Color.b);
    DrawRectangle(Buffer, V2(vMin.x - T, vMin.y - T), V2(vMax.x + T, vMin.y + T), Color.r, Color.g, Color.b);
    DrawRectangle(Buffer, V2(vMin.x - T, vMin.y - T), V2(vMin.x + T, vMax.y + T), Color.r, Color.g, Color.b);
    DrawRectangle(Buffer, V2(vMax.x - T, vMin.y - T), V2(vMax.x + T, vMax.y + T), Color.r, Color.g, Color.b);
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

    v2 ScreenCenter = 0.5f * V2((real32)OutputTarget->Width, (real32)OutputTarget->Height);

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
                Entry->Color.r, Entry->Color.g, Entry->Color.b, Entry->Color.a
            );

            BaseAddress += sizeof(*Entry);
        } break;

        case RenderGroupEntryType_render_entry_bitmap: {
            render_entry_bitmap* Entry = (render_entry_bitmap*)Data;
            v2 P = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis, ScreenCenter);
            Assert(Entry->Bitmap);
            DrawBitmap(OutputTarget, Entry->Bitmap, P.x, P.y, Entry->A);
            BaseAddress += sizeof(*Entry);
        } break;

        case RenderGroupEntryType_render_entry_rectangle: {
            render_entry_rectangle* Entry = (render_entry_rectangle*)Data;
            v2 P = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis, ScreenCenter);
            DrawRectangle(OutputTarget, P, P + Entry->Dim, Entry->R, Entry->G, Entry->B);
            BaseAddress += sizeof(*Entry);
        } break;

        case RenderGroupEntryType_render_entry_coordinate_system: {
            render_entry_coordinate_system* Entry = (render_entry_coordinate_system*)Data;

            v2 P = Entry->Origin;
            v2 Dim = V2(2, 2);

            v2 PX = P + Entry->XAxis;
            v2 PY = P + Entry->YAxis;

            DrawRectangle(OutputTarget, P - Dim, P + Dim, Entry->Color.r, Entry->Color.g, Entry->Color.b);
            DrawRectangle(OutputTarget, PX - Dim, PX + Dim, Entry->Color.r, Entry->Color.g, Entry->Color.b);
            DrawRectangle(OutputTarget, PY - Dim, PY + Dim, Entry->Color.r, Entry->Color.g, Entry->Color.b);
            DrawRectangleSlowly(OutputTarget, Entry->Origin, Entry->XAxis, Entry->YAxis, Entry->Color, Entry->Texture);

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
