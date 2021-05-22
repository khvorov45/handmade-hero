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

struct high_entity {
    bool32 Exists;
    v2 P; //* Relative to the camera
    v2 dP;
    uint32 AbsTileZ;
    uint32 FacingDirection;

    real32 Z;
    real32 dZ;
};

struct low_entity {};

struct dormant_entity {
    tile_map_position P;
    real32 Width;
    real32 Height;
    int32 dAbsTileZ; //* Stairs
    bool32 Collides;
};

struct entity {
    uint32 Residence;
    low_entity* Low;
    dormant_entity* Dormant;
    high_entity* High;
};

enum entity_residence {
    EntityResidence_Nonexistant,
    EntityResidence_Dormant,
    EntityResidence_Low,
    EntityResidence_High
};

struct game_state {
    memory_arena WorldArena;
    world* World;
    uint32 CameraFollowingEntityIndex;
    tile_map_position CameraP;

    uint32 PlayerIndexForController[ArrayCount(((game_input*)0)->Controllers)];

    uint32 EntityCount;
    entity_residence EntityResidence[256];
    high_entity HighEntities[256];
    low_entity LowEntities[256];
    dormant_entity DormantEntities[256];

    loaded_bmp Backdrop;
    hero_bitmaps HeroBitmaps[4];
    loaded_bmp HeroShadow;
};
