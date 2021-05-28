#if !defined(HANDMADE_TILE_CPP)
#define HANDMADE_TILE_CPP

#include "../intrinsics.H"
#include "../types.h"
#include "../util.h"
#include "memory.cpp"
#include "math.cpp"

struct tile_map_position {
    //* High bits - tile chunk index
    //* Low bits - tile location in the chunk
    int32 AbsTileX;
    int32 AbsTileY;
    int32 AbsTileZ;

    //* Offset from the center
    v2 Offset_;
};

struct tile_entity_block {
    uint32 EntityCount;
    uint32 LowEntityIndex[16];
    tile_entity_block* Next;
};

struct world_chunk {
    int32 ChunkX;
    int32 ChunkY;
    int32 ChunkZ;

    tile_entity_block FirstBlock;

    world_chunk* NextInHash;
};

struct world_chunk_position {
    int32 TileChunkX;
    int32 TileChunkY;
    int32 TileChunkZ;

    int32 RelTileX;
    int32 RelTileY;
};

struct world {
    real32 TileSideInMeters;

    uint32 WorldChunkShift;
    uint32 WorldChunkMask;
    int32 WorldChunkDim;
    world_chunk WorldChunkHash[4096];
};

#if 0
internal inline uint32 GetTileChunkValueUnchecked(
    world* TileMap, world_chunk* TileChunk, int32 TileX, int32 TileY
) {
    Assert(TileChunk != 0);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);
    return TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX];
}

internal inline void SetTileChunkValueUnchecked(
    world* TileMap, world_chunk* TileChunk, int32 TileX, int32 TileY, uint32 TileValue
) {
    Assert(TileChunk != 0);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);
    TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX] = TileValue;
}
internal uint32 GetTileChunkValue(
    world* TileMap,
    world_chunk* TileChunk,
    uint32 TestTileX, uint32 TestTileY
) {
    if (TileChunk == 0 || TileChunk->Tiles == 0) {
        return 0;
    }
    return GetTileChunkValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY);
}
#endif

#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX / 64)
#define TILE_CHUNK_UNINIT INT32_MAX

internal inline world_chunk* GetTileChunk(
    world* TileMap, int32 TileChunkX, int32 TileChunkY, int32 TileChunkZ,
    memory_arena* Arena = 0
) {
    Assert(TileChunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkZ > -TILE_CHUNK_SAFE_MARGIN);

    Assert(TileChunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkY < TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkZ < TILE_CHUNK_SAFE_MARGIN);

    uint32 HashValue = 19 * TileChunkX + 7 * TileChunkY + 3 * TileChunkZ;
    uint32 HashSlot = HashValue & (ArrayCount(TileMap->WorldChunkHash) - 1);
    Assert(HashSlot < ArrayCount(TileMap->WorldChunkHash));

    world_chunk* Chunk = TileMap->WorldChunkHash + HashSlot;

    while (Chunk != 0) {
        if ((TileChunkX == Chunk->ChunkX) &&
            (TileChunkY == Chunk->ChunkY) &&
            (TileChunkZ == Chunk->ChunkY)) {
            break;
        }

        if (Arena != 0 && Chunk->ChunkX != TILE_CHUNK_UNINIT && Chunk->NextInHash == 0) {
            Chunk->NextInHash = PushStruct(Arena, world_chunk);
            Chunk = Chunk->NextInHash;
            Chunk->ChunkX = TILE_CHUNK_UNINIT;
        }

        if (Arena != 0 && Chunk->ChunkX == TILE_CHUNK_UNINIT) {
            uint32 TileCount = TileMap->WorldChunkDim * TileMap->WorldChunkDim;

            Chunk->ChunkX = TileChunkX;
            Chunk->ChunkY = TileChunkY;
            Chunk->ChunkZ = TileChunkZ;

            Chunk->NextInHash = 0;
            break;
        }
        Chunk = Chunk->NextInHash;
    }

    return Chunk;
}

#if 0
internal void SetTileValue(
    world* TileMap,
    world_chunk* TileChunk,
    uint32 TestTileX, uint32 TestTileY,
    uint32 TileValue
) {
    if (TileChunk == 0 || TileChunk->Tiles == 0) {
        return;
    }

    return SetTileChunkValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY, TileValue);
}
#endif

internal inline world_chunk_position GetChunkPositionFor(world* TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_chunk_position Result;
    Result.TileChunkX = AbsTileX >> TileMap->WorldChunkShift;
    Result.TileChunkY = AbsTileY >> TileMap->WorldChunkShift;
    Result.TileChunkZ = AbsTileZ;
    Result.RelTileX = AbsTileX & TileMap->WorldChunkMask;
    Result.RelTileY = AbsTileY & TileMap->WorldChunkMask;
    return Result;
}

#if 0
internal uint32 GetTileValue(world* TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    world_chunk* TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ);
    uint32 TileValue = GetTileChunkValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY);
    return TileValue;
}

internal uint32 GetTileValue(world* TileMap, tile_map_position Pos) {
    return GetTileValue(TileMap, Pos.AbsTileX, Pos.AbsTileY, Pos.AbsTileZ);
}
#endif

internal bool32 IsTileValueEmpty(uint32 TileValue) {
    return TileValue == 1 || TileValue == 3 || TileValue == 4;
}

#if 0
internal bool32 IsTileMapPointEmpty(world* TileMap, tile_map_position CanonicalPos) {
    uint32 TileValue = GetTileValue(TileMap, CanonicalPos.AbsTileX, CanonicalPos.AbsTileY, CanonicalPos.AbsTileZ);
    return IsTileValueEmpty(TileValue);
}
#endif

internal inline void RecanonicalizeCoord(world* TileMap, int32* Tile, real32* TileRel) {
    int32 Offset = RoundReal32ToInt32((*TileRel) / TileMap->TileSideInMeters);

    *Tile += Offset;
    *TileRel -= Offset * TileMap->TileSideInMeters;

    //! Sometimes it's right on
    Assert(*TileRel > -0.5f * TileMap->TileSideInMeters);
    Assert(*TileRel < 0.5f * TileMap->TileSideInMeters);
}

internal inline tile_map_position MapIntoTileSpace(world* TileMap, tile_map_position BasePos, v2 Offset) {
    tile_map_position Result = BasePos;

    Result.Offset_ += Offset;
    RecanonicalizeCoord(TileMap, &Result.AbsTileX, &Result.Offset_.X);
    RecanonicalizeCoord(TileMap, &Result.AbsTileY, &Result.Offset_.Y);

    return Result;
}

#if 0
internal void SetTileValue(
    memory_arena* Arena, world* TileMap,
    uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ,
    uint32 TileValue
) {
    world_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    world_chunk* TileChunk =
        GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ, Arena);
    SetTileValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY, TileValue);
}
#endif

internal bool32 AreOnSameTile(tile_map_position* Pos1, tile_map_position* Pos2) {
    return Pos1->AbsTileX == Pos2->AbsTileX &&
        Pos1->AbsTileY == Pos2->AbsTileY &&
        Pos1->AbsTileZ == Pos2->AbsTileZ;
}

struct tile_map_difference {
    v2 dXY;
    real32 dZ;
};

tile_map_difference Subtract(world* TileMap, tile_map_position* A, tile_map_position* B) {
    tile_map_difference Result = {};

    v2 dTileXY = { (real32)A->AbsTileX - (real32)B->AbsTileX, (real32)A->AbsTileY - (real32)B->AbsTileY };

    real32 dTileZ = TileMap->TileSideInMeters * ((real32)A->AbsTileZ - (real32)B->AbsTileZ);

    Result.dXY = TileMap->TileSideInMeters * dTileXY + (A->Offset_ - B->Offset_);

    Result.dZ = dTileZ;

    return Result;
}

internal tile_map_position CenteredTilePoint(uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    tile_map_position Pos;
    Pos.AbsTileX = AbsTileX;
    Pos.AbsTileY = AbsTileY;
    Pos.AbsTileZ = AbsTileZ;
    Pos.Offset_ = { 0.0f, 0.0f };
    return Pos;
}

internal void InitializeTileMap(world* TileMap, real32 TileSideInMeters) {
    TileMap->WorldChunkShift = 4;
    TileMap->WorldChunkMask = (1 << TileMap->WorldChunkShift) - 1;
    TileMap->WorldChunkDim = (1 << TileMap->WorldChunkShift);

    TileMap->TileSideInMeters = TileSideInMeters;

    for (uint32 TileChunkIndex = 0; TileChunkIndex < ArrayCount(TileMap->WorldChunkHash); ++TileChunkIndex) {
        TileMap->WorldChunkHash[TileChunkIndex].ChunkX = TILE_CHUNK_UNINIT;
    }
}

#endif
