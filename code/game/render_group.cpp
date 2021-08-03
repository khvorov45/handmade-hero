#include "math.cpp"
#include "bmp.cpp"
#include "sim_region.cpp"

struct render_basis {
    v3 P;
};

struct entity_visible_piece {
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

internal inline void
PushPiece(
    render_group* Group, loaded_bitmap* Bitmap,
    v2 Offset, real32 OffsetZ, v2 Align, v2 Dim,
    v4 Color,
    real32 EntityZC = 1.0f
) {
    real32 MetersToPixels = Group->MetersToPixels;
    entity_visible_piece* Piece =
        (entity_visible_piece*)PushRenderElement(Group, sizeof(entity_visible_piece));
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
