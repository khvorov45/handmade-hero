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
    render_group_entry_header Header;
    v4 Color;
};

struct render_entry_coordinate_system {
    render_group_entry_header Header;
    v2 Origin;
    v2 XAxis;
    v2 YAxis;
    v4 Color;

    v2 Points[16];
};

struct render_entry_bitmap {
    render_group_entry_header Header;
    loaded_bitmap* Bitmap;
    render_entity_basis EntityBasis;
    real32 R, G, B, A;
};

struct render_entry_rectangle {
    render_group_entry_header Header;
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

inline render_group_entry_header*
PushRenderElement_(render_group* Group, uint32 Size, render_group_entry_type Type) {
    render_group_entry_header* Result = 0;
    if (Group->PushBufferSize + Size < Group->MaxPushBufferSize) {
        Result = (render_group_entry_header*)(Group->PushBufferBase + Group->PushBufferSize);
        Result->Type = Type;
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

internal void CoordinateSystem(render_group* Group, v2 Origin, v2 XAxis, v2 YAxis, v4 Color) {
    render_entry_coordinate_system* Entry = PushRenderElement(Group, render_entry_coordinate_system);
    if (Entry) {
        Entry->Origin = Origin;
        Entry->XAxis = XAxis;
        Entry->YAxis = YAxis;
        Entry->Color = Color;
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

/// Maximums are not inclusive
internal void DrawRectangleSlowly(
    loaded_bitmap* Buffer,
    v2 Origin, v2 XAxis, v2 YAxis,
    v4 Color
) {
    uint32 Color32 =
        (RoundReal32ToUint32(Color.a * 255.0f) << 24) |
        (RoundReal32ToUint32(Color.r * 255.0f) << 16) |
        (RoundReal32ToUint32(Color.g * 255.0f) << 8) |
        (RoundReal32ToUint32(Color.b * 255.0f));

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

    uint8* Row = (uint8*)Buffer->Memory + YMin * Buffer->Pitch + XMin * BITMAP_BYTES_PER_PIXEL;
    for (int32 Y = YMin; Y <= YMax; ++Y) {
        uint32* Pixel = (uint32*)Row;
        for (int32 X = XMin; X <= XMax; ++X) {
            v2 PixelP = V2i(X, Y);
            real32 Edge0 = Inner(PixelP - Origin, -YAxis);
            real32 Edge1 = Inner(PixelP - (Origin + XAxis), XAxis);
            real32 Edge2 = Inner(PixelP - (Origin + XAxis + YAxis), YAxis);
            real32 Edge3 = Inner(PixelP - (Origin + YAxis), -XAxis);
            if (Edge0 < 0 && Edge1 < 0 && Edge2 < 0 && Edge3 < 0) {
                *Pixel = Color32;
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
            real32 RA = (SA01 + DA01 - SA01 * DA01) * 255.0f;
            real32 RR = InvSA01 * DR + SR;
            real32 RG = InvSA01 * DG + SG;
            real32 RB = InvSA01 * DB + SB;

            *Dest = (RoundReal32ToUint32(RA) << 24) | (RoundReal32ToUint32(RR) << 16) |
                (RoundReal32ToUint32(RG) << 8) | (RoundReal32ToUint32(RB));

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

        render_group_entry_header* TypelessEntry =
            (render_group_entry_header*)(RenderGroup->PushBufferBase + BaseAddress);

        switch (TypelessEntry->Type) {
        case RenderGroupEntryType_render_entry_clear: {
            render_entry_clear* Entry = (render_entry_clear*)TypelessEntry;

            DrawRectangle(
                OutputTarget, V2(0, 0), V2i(OutputTarget->Width, OutputTarget->Height),
                Entry->Color.r, Entry->Color.g, Entry->Color.b, Entry->Color.a
            );

            BaseAddress += sizeof(*Entry);
        } break;

        case RenderGroupEntryType_render_entry_bitmap: {
            render_entry_bitmap* Entry = (render_entry_bitmap*)TypelessEntry;
            v2 P = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis, ScreenCenter);
            Assert(Entry->Bitmap);
            DrawBitmap(OutputTarget, Entry->Bitmap, P.x, P.y, Entry->A);
            BaseAddress += sizeof(*Entry);
        } break;

        case RenderGroupEntryType_render_entry_rectangle: {
            render_entry_rectangle* Entry = (render_entry_rectangle*)TypelessEntry;
            v2 P = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis, ScreenCenter);
            DrawRectangle(OutputTarget, P, P + Entry->Dim, Entry->R, Entry->G, Entry->B);
            BaseAddress += sizeof(*Entry);
        } break;

        case RenderGroupEntryType_render_entry_coordinate_system: {
            render_entry_coordinate_system* Entry = (render_entry_coordinate_system*)TypelessEntry;

            v2 P = Entry->Origin;
            v2 Dim = V2(2, 2);

            v2 PX = P + Entry->XAxis;
            v2 PY = P + Entry->YAxis;

            DrawRectangle(OutputTarget, P - Dim, P + Dim, Entry->Color.r, Entry->Color.g, Entry->Color.b);
            DrawRectangle(OutputTarget, PX - Dim, PX + Dim, Entry->Color.r, Entry->Color.g, Entry->Color.b);
            DrawRectangle(OutputTarget, PY - Dim, PY + Dim, Entry->Color.r, Entry->Color.g, Entry->Color.b);
            DrawRectangleSlowly(OutputTarget, Entry->Origin, Entry->XAxis, Entry->YAxis, Entry->Color);

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
