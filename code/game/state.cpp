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
    uint32 ChunkZ;
    uint32 FacingDirection;

    real32 Z;
    real32 dZ;

    uint32 LowEntityIndex;
};

enum entity_type {
    EntityType_Null,
    EntityType_Hero,
    EntityType_Wall
};

struct low_entity {
    entity_type Type;
    world_position P;
    real32 Width;
    real32 Height;
    int32 dAbsTileZ; //* Stairs
    bool32 Collides;

    uint32 HighEntityIndex;
};

struct entity {
    uint32 LowIndex;
    low_entity* Low;
    high_entity* High;
};

struct low_entity_chunk_reference {
    world_chunk* TileChunk;
    uint32 EntityIndexInChunk;
};

struct game_state {
    memory_arena WorldArena;
    world* World;
    uint32 CameraFollowingEntityIndex;
    world_position CameraP;

    uint32 PlayerIndexForController[ArrayCount(((game_input*)0)->Controllers)];

    uint32 HighEntityCount;
    high_entity HighEntities_[256];

    uint32 LowEntityCount;
    low_entity LowEntities_[1000000];

    loaded_bmp Backdrop;
    hero_bitmaps HeroBitmaps[4];
    loaded_bmp HeroShadow;
};
