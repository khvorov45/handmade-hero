#include "tile.cpp"
#include "world.cpp"

struct game_state {
#if 0
    real32 PlayerX;
    real32 PlayerY;

    int32 PlayerTileMapX;
    int32 PlayerTileMapY;
#endif
    world* World;
    tile_map_position PlayerP;
};
