#include "math.cpp"
#include "bmp.cpp"
#include "sim_region.cpp"

struct entity_visible_piece {
    loaded_bitmap* Bitmap;
    v2 Offset;
    real32 OffsetZ;
    real32 EntityZC;

    real32 R, G, B, A;
    v2 Dim;
};

struct entity_visible_piece_group {
    game_state* GameState;
    uint32 PieceCount;
    entity_visible_piece Pieces[32];
};
