#if !defined(HANDMADE_TILE_CPP)
#define HANDMADE_TILE_CPP

#include "../intrinsics.H"
#include "../types.h"
#include "../util.h"
#include "memory.cpp"
#include "math.cpp"

struct world_position {
    //* High bits - tile chunk index
    //* Low bits - tile location in the chunk
    int32 AbsTileX;
    int32 AbsTileY;
    int32 AbsTileZ;

    //* Offset from the center
    v2 Offset_;
};

struct world_difference {
    v2 dXY;
    real32 dZ;
};

struct world_entity_block {
    uint32 EntityCount;
    uint32 LowEntityIndex[16];
    world_entity_block* Next;
};

struct world_chunk {
    int32 ChunkX;
    int32 ChunkY;
    int32 ChunkZ;

    world_entity_block FirstBlock;

    world_chunk* NextInHash;
};

#if 0
struct world_chunk_position {
    int32 WorldChunkX;
    int32 WorldChunkY;
    int32 WorldChunkZ;

    int32 RelTileX;
    int32 RelTileY;
};
#endif

struct world {
    real32 TileSideInMeters;

    uint32 WorldChunkShift;
    uint32 WorldChunkMask;
    int32 WorldChunkDim;
    world_chunk WorldChunkHash[4096];
};

#if 0
internal inline uint32 GetWorldChunkValueUnchecked(
    world* World, world_chunk* WorldChunk, int32 TileX, int32 TileY
) {
    Assert(WorldChunk != 0);
    Assert(TileX < World->ChunkDim);
    Assert(TileY < World->ChunkDim);
    return WorldChunk->Tiles[TileY * World->ChunkDim + TileX];
}

internal inline void SetWorldChunkValueUnchecked(
    world* World, world_chunk* WorldChunk, int32 TileX, int32 TileY, uint32 TileValue
) {
    Assert(WorldChunk != 0);
    Assert(TileX < World->ChunkDim);
    Assert(TileY < World->ChunkDim);
    WorldChunk->Tiles[TileY * World->ChunkDim + TileX] = TileValue;
}
internal uint32 GetWorldChunkValue(
    world* World,
    world_chunk* WorldChunk,
    uint32 TestTileX, uint32 TestTileY
) {
    if (WorldChunk == 0 || WorldChunk->Tiles == 0) {
        return 0;
    }
    return GetWorldChunkValueUnchecked(World, WorldChunk, TestTileX, TestTileY);
}
#endif

#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX / 64)
#define TILE_CHUNK_UNINIT INT32_MAX

internal inline world_chunk* GetWorldChunk(
    world* World, int32 WorldChunkX, int32 WorldChunkY, int32 WorldChunkZ,
    memory_arena* Arena = 0
) {
    Assert(WorldChunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkZ > -TILE_CHUNK_SAFE_MARGIN);

    Assert(WorldChunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkY < TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkZ < TILE_CHUNK_SAFE_MARGIN);

    uint32 HashValue = 19 * WorldChunkX + 7 * WorldChunkY + 3 * WorldChunkZ;
    uint32 HashSlot = HashValue & (ArrayCount(World->WorldChunkHash) - 1);
    Assert(HashSlot < ArrayCount(World->WorldChunkHash));

    world_chunk* Chunk = World->WorldChunkHash + HashSlot;

    while (Chunk != 0) {
        if ((WorldChunkX == Chunk->ChunkX) &&
            (WorldChunkY == Chunk->ChunkY) &&
            (WorldChunkZ == Chunk->ChunkY)) {
            break;
        }

        if (Arena != 0 && Chunk->ChunkX != TILE_CHUNK_UNINIT && Chunk->NextInHash == 0) {
            Chunk->NextInHash = PushStruct(Arena, world_chunk);
            Chunk = Chunk->NextInHash;
            Chunk->ChunkX = TILE_CHUNK_UNINIT;
        }

        if (Arena != 0 && Chunk->ChunkX == TILE_CHUNK_UNINIT) {
            uint32 TileCount = World->WorldChunkDim * World->WorldChunkDim;

            Chunk->ChunkX = WorldChunkX;
            Chunk->ChunkY = WorldChunkY;
            Chunk->ChunkZ = WorldChunkZ;

            Chunk->NextInHash = 0;
            break;
        }
        Chunk = Chunk->NextInHash;
    }

    return Chunk;
}

#if 0
internal void SetTileValue(
    world* World,
    world_chunk* WorldChunk,
    uint32 TestTileX, uint32 TestTileY,
    uint32 TileValue
) {
    if (WorldChunk == 0 || WorldChunk->Tiles == 0) {
        return;
    }

    return SetWorldChunkValueUnchecked(World, WorldChunk, TestTileX, TestTileY, TileValue);
}

internal inline world_chunk_position GetChunkPositionFor(world* World, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_chunk_position Result;
    Result.WorldChunkX = AbsTileX >> World->WorldChunkShift;
    Result.WorldChunkY = AbsTileY >> World->WorldChunkShift;
    Result.WorldChunkZ = AbsTileZ;
    Result.RelTileX = AbsTileX & World->WorldChunkMask;
    Result.RelTileY = AbsTileY & World->WorldChunkMask;
    return Result;
}

internal uint32 GetTileValue(world* World, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_chunk_position ChunkPos = GetChunkPositionFor(World, AbsTileX, AbsTileY, AbsTileZ);
    world_chunk* WorldChunk = GetWorldChunk(World, ChunkPos.WorldChunkX, ChunkPos.WorldChunkY, ChunkPos.WorldChunkZ);
    uint32 TileValue = GetWorldChunkValue(World, WorldChunk, ChunkPos.RelTileX, ChunkPos.RelTileY);
    return TileValue;
}

internal uint32 GetTileValue(world* World, world_position Pos) {
    return GetTileValue(World, Pos.AbsTileX, Pos.AbsTileY, Pos.AbsTileZ);
}
#endif

internal bool32 IsTileValueEmpty(uint32 TileValue) {
    return TileValue == 1 || TileValue == 3 || TileValue == 4;
}

#if 0
internal bool32 IsWorldPointEmpty(world* World, world_position CanonicalPos) {
    uint32 TileValue = GetTileValue(World, CanonicalPos.AbsTileX, CanonicalPos.AbsTileY, CanonicalPos.AbsTileZ);
    return IsTileValueEmpty(TileValue);
}
#endif

internal inline void RecanonicalizeCoord(world* World, int32* Tile, real32* TileRel) {
    int32 Offset = RoundReal32ToInt32((*TileRel) / World->TileSideInMeters);

    *Tile += Offset;
    *TileRel -= Offset * World->TileSideInMeters;

    //! Sometimes it's right on
    Assert(*TileRel > -0.5f * World->TileSideInMeters);
    Assert(*TileRel < 0.5f * World->TileSideInMeters);
}

internal inline world_position MapIntoTileSpace(world* World, world_position BasePos, v2 Offset) {
    world_position Result = BasePos;

    Result.Offset_ += Offset;
    RecanonicalizeCoord(World, &Result.AbsTileX, &Result.Offset_.X);
    RecanonicalizeCoord(World, &Result.AbsTileY, &Result.Offset_.Y);

    return Result;
}

#if 0
internal void SetTileValue(
    memory_arena* Arena, world* World,
    uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ,
    uint32 TileValue
) {
    world_chunk_position ChunkPos = GetChunkPositionFor(World, AbsTileX, AbsTileY, AbsTileZ);
    world_chunk* WorldChunk =
        GetWorldChunk(World, ChunkPos.WorldChunkX, ChunkPos.WorldChunkY, ChunkPos.WorldChunkZ, Arena);
    SetTileValue(World, WorldChunk, ChunkPos.RelTileX, ChunkPos.RelTileY, TileValue);
}
#endif

internal bool32 AreOnSameTile(world_position* Pos1, world_position* Pos2) {
    return Pos1->AbsTileX == Pos2->AbsTileX &&
        Pos1->AbsTileY == Pos2->AbsTileY &&
        Pos1->AbsTileZ == Pos2->AbsTileZ;
}

world_difference Subtract(world* World, world_position* A, world_position* B) {
    world_difference Result = {};

    v2 dTileXY = { (real32)A->AbsTileX - (real32)B->AbsTileX, (real32)A->AbsTileY - (real32)B->AbsTileY };

    real32 dTileZ = World->TileSideInMeters * ((real32)A->AbsTileZ - (real32)B->AbsTileZ);

    Result.dXY = World->TileSideInMeters * dTileXY + (A->Offset_ - B->Offset_);

    Result.dZ = dTileZ;

    return Result;
}

internal world_position CenteredTilePoint(uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position Pos;
    Pos.AbsTileX = AbsTileX;
    Pos.AbsTileY = AbsTileY;
    Pos.AbsTileZ = AbsTileZ;
    Pos.Offset_ = { 0.0f, 0.0f };
    return Pos;
}

internal void InitializeWorld(world* World, real32 TileSideInMeters) {
    World->WorldChunkShift = 4;
    World->WorldChunkMask = (1 << World->WorldChunkShift) - 1;
    World->WorldChunkDim = (1 << World->WorldChunkShift);

    World->TileSideInMeters = TileSideInMeters;

    for (uint32 WorldChunkIndex = 0; WorldChunkIndex < ArrayCount(World->WorldChunkHash); ++WorldChunkIndex) {
        World->WorldChunkHash[WorldChunkIndex].ChunkX = TILE_CHUNK_UNINIT;
    }
}

#endif
