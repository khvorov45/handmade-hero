#include "math.cpp"
#include "bmp.cpp"
#include "sim_region.cpp"

struct render_basis {
    v3 P;
};

struct render_group_entry {
    render_basis* Basis;
    loaded_bitmap* Bitmap;
    v2 Offset;
    real32 OffsetZ;
    real32 EntityZC;

    real32 R, G, B, A;
    v2 Dim;
};

struct render_group {
    render_basis* DefaultBasis;
    real32 MetersToPixels;
    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8* PushBufferBase;
};

inline void* PushRenderElement(render_group* Group, uint32 Size) {
    void* Result = 0;
    if (Group->PushBufferSize + Size < Group->MaxPushBufferSize) {
        Result = Group->PushBufferBase + Group->PushBufferSize;
        Group->PushBufferSize += Size;
    } else {
        InvalidCodePath;
    }
    return Result;
}

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


internal inline void
PushPiece(
    render_group* Group, loaded_bitmap* Bitmap,
    v2 Offset, real32 OffsetZ, v2 Align, v2 Dim,
    v4 Color,
    real32 EntityZC = 1.0f
) {
    real32 MetersToPixels = Group->MetersToPixels;
    render_group_entry* Piece =
        (render_group_entry*)PushRenderElement(Group, sizeof(render_group_entry));
    v2 OffsetFixY = { Offset.X, -Offset.Y };
    Piece->Basis = Group->DefaultBasis;
    Piece->R = Color.R;
    Piece->G = Color.G;
    Piece->B = Color.B;
    Piece->A = Color.A;
    Piece->Bitmap = Bitmap;
    Piece->Offset = MetersToPixels * OffsetFixY - Align;
    Piece->OffsetZ = OffsetZ;
    Piece->EntityZC = EntityZC;
    Piece->Dim = Dim;
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

internal void
PushRect(
    render_group* Group,
    v2 Offset, real32 OffsetZ, v2 Dim,
    v4 Color,
    real32 EntityZC = 1.0f
) {
    PushPiece(
        Group, 0, Offset, OffsetZ, { 0, 0 }, Dim, Color, EntityZC
    );
}

internal void
PushRectOutline(
    render_group* Group,
    v2 Offset, real32 OffsetZ, v2 Dim,
    v4 Color,
    real32 EntityZC = 1.0f
) {
    real32 Thickness = 0.1f;

    //* Top and bottom
    PushPiece(Group, 0, Offset - V2(0, 0.5f * Dim.Y), OffsetZ, { 0, 0 }, V2(Dim.X, Thickness), Color, EntityZC);
    PushPiece(Group, 0, Offset + V2(0, 0.5f * Dim.Y), OffsetZ, { 0, 0 }, V2(Dim.X, Thickness), Color, EntityZC);

    //* Left and right
    PushPiece(Group, 0, Offset - V2(0.5f * Dim.X, 0), OffsetZ, { 0, 0 }, V2(Thickness, Dim.Y), Color, EntityZC);
    PushPiece(Group, 0, Offset + V2(0.5f * Dim.X, 0), OffsetZ, { 0, 0 }, V2(Thickness, Dim.Y), Color, EntityZC);
}

/// Maximums are not inclusive
internal void DrawRectangle(
    loaded_bitmap* Buffer,
    v2 vMin, v2 vMax,
    real32 R, real32 G, real32 B
) {
    int32 MinX = RoundReal32ToInt32(vMin.X);
    int32 MinY = RoundReal32ToInt32(vMin.Y);
    int32 MaxX = RoundReal32ToInt32(vMax.X);
    int32 MaxY = RoundReal32ToInt32(vMax.Y);

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

    uint32 Color = (RoundReal32ToUint32(R * 255.0f) << 16) |
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

internal void
DrawRectangleOutline(
    loaded_bitmap* Buffer,
    v2 vMin, v2 vMax,
    v3 Color,
    real32 T = 2.0f
) {
    DrawRectangle(Buffer, V2(vMin.X - T, vMax.Y - T), V2(vMax.X + T, vMax.Y + T), Color.R, Color.G, Color.B);
    DrawRectangle(Buffer, V2(vMin.X - T, vMin.Y - T), V2(vMax.X + T, vMin.Y + T), Color.R, Color.G, Color.B);
    DrawRectangle(Buffer, V2(vMin.X - T, vMin.Y - T), V2(vMin.X + T, vMax.Y + T), Color.R, Color.G, Color.B);
    DrawRectangle(Buffer, V2(vMax.X - T, vMin.Y - T), V2(vMax.X + T, vMax.Y + T), Color.R, Color.G, Color.B);
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
