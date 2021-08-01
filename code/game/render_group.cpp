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

struct entity_visible_piece_group {
    render_basis* DefaultBasis;
    game_state* GameState;
    uint32 PieceCount;
    entity_visible_piece Pieces[4096];
};
