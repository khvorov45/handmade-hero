#include "tile.cpp"
#include "world.cpp"
#include "memory.cpp"
#include "bmp.cpp"
#include "math.cpp"

struct hero_bitmaps {
    int32 AlignX;
    int32 AlignY;
    loaded_bmp Head;
    loaded_bmp Cape;
    loaded_bmp Torso;
};

struct entity {
    bool32 Exists;
    tile_map_position P;
    v2 dP;
    uint32 FacingDirection;
    real32 Width;
    real32 Height;
};

struct game_state {
    memory_arena WorldArena;
    world* World;
    uint32 CameraFollowingEntityIndex;
    tile_map_position CameraP;

    uint32 PlayerIndexForController[ArrayCount(((game_input*)0)->Controllers)];
    uint32 EntityCount;
    entity Entities[256];

    loaded_bmp Backdrop;
    hero_bitmaps HeroBitmaps[4];
};
