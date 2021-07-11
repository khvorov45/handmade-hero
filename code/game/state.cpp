#if !defined(HANDMADE_STATE_CPP)
#define HANDMADE_STATE_CPP

#include "world.cpp"
#include "memory.cpp"
#include "bmp.cpp"
#include "math.cpp"

struct hero_bitmaps {
    v2 Align;
    loaded_bitmap Head;
    loaded_bitmap Cape;
    loaded_bitmap Torso;
};

struct high_entity {
    bool32 Exists;
    v2 P; //* Relative to the camera
    v2 dP;
    uint32 ChunkZ;
    uint32 FacingDirection;

    real32 tBob;

    real32 Z;
    real32 dZ;

    uint32 LowEntityIndex;
};

enum entity_type {
    EntityType_Null,

    EntityType_Space,

    EntityType_Hero,
    EntityType_Wall,
    EntityType_Familiar,
    EntityType_Monster,
    EntityType_Sword,
    EntityType_Stairwell
};

#define HITPOINT_SUBCOUNT 4
struct hit_point {
    uint8 Flags;
    uint8 FilledAmount;
};

struct low_entity {
    entity_type Type;
    world_position P;
    real32 Width;
    real32 Height;
    int32 dAbsTileZ; //* Stairs
    bool32 Collides;

    uint32 HighEntityIndex;

    uint32 HitPointMax;
    hit_point HitPoint[16];

    uint32 SwordLowIndex;
    real32 DistanceRemaining;
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

struct entity_visible_piece {
    loaded_bitmap* Bitmap;
    v2 Offset;
    real32 OffsetZ;
    real32 EntityZC;

    real32 R, G, B, A;
    v2 Dim;
};

struct controlled_hero {
    uint32 EntityIndex;
    v2 ddP;
    v2 dSword;
    real32 dZ;
};

#endif
