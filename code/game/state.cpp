#include "tile.cpp"
#include "world.cpp"
#include "memory.cpp"
#include "bmp.cpp"

struct hero_bitmaps {
    int32 AlignX;
    int32 AlignY;
    loaded_bmp Head;
    loaded_bmp Cape;
    loaded_bmp Torso;
};

struct game_state {
    memory_arena WorldArena;
    world* World;
    tile_map_position CameraP;
    tile_map_position PlayerP;
    loaded_bmp Backdrop;
    uint32 HeroFacingDirection;
    hero_bitmaps HeroBitmaps[4];
};
