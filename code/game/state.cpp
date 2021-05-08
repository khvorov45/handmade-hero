#include "tile.cpp"
#include "world.cpp"
#include "memory.cpp"
#include "bmp.cpp"

struct game_state {
    memory_arena WorldArena;
    world* World;
    tile_map_position PlayerP;
    loaded_bmp Backdrop;
};
