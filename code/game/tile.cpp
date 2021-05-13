#if !defined(HANDMADE_TILE_CPP)
#define HANDMADE_TILE_CPP

#include "../types.h"
#include "../util.h"
#include "memory.cpp"
#include "math.cpp"

struct tile_map_position {
    //* High bits - tile chunk index
    //* Low bits - tile location in the chunk
    uint32 AbsTileX;
    uint32 AbsTileY;
    uint32 AbsTileZ;

    //* Offset from the center
    v2 Offset;
};

struct tile_chunk {
    uint32* Tiles;
};

struct tile_chunk_position {
    uint32 TileChunkX;
    uint32 TileChunkY;
    uint32 TileChunkZ;

    uint32 RelTileX;
    uint32 RelTileY;
};

struct tile_map {
    uint32 ChunkShift;
    uint32 ChunkMask;
    uint32 ChunkDim;

    real32 TileSideInMeters;

    uint32 TileChunkCountX;
    uint32 TileChunkCountY;
    uint32 TileChunkCountZ;

    tile_chunk* TileChunks;
};

internal inline uint32 GetTileChunkValueUnchecked(tile_map* TileMap, tile_chunk* TileChunk, uint32 TileX, uint32 TileY) {
    Assert(TileChunk != 0);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);
    return TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX];
}

internal inline void SetTileChunkValueUnchecked(
    tile_map* TileMap, tile_chunk* TileChunk, uint32 TileX, uint32 TileY, uint32 TileValue
) {
    Assert(TileChunk != 0);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);
    TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX] = TileValue;
}

internal inline tile_chunk* GetTileChunk(tile_map* TileMap, uint32 TileChunkX, uint32 TileChunkY, uint32 TileChunkZ) {
    tile_chunk* TileChunk = 0;
    if ((TileChunkX >= 0) && (TileChunkX < TileMap->TileChunkCountX) &&
        (TileChunkY >= 0) && (TileChunkY < TileMap->TileChunkCountY) &&
        (TileChunkZ >= 0) && (TileChunkZ < TileMap->TileChunkCountZ)) {
        TileChunk = &TileMap->TileChunks[TileChunkZ * TileMap->TileChunkCountY * TileMap->TileChunkCountX + TileChunkY * TileMap->TileChunkCountX + TileChunkX];
    }
    return TileChunk;
}

internal uint32 GetTileChunkValue(
    tile_map* TileMap,
    tile_chunk* TileChunk,
    uint32 TestTileX, uint32 TestTileY
) {
    if (TileChunk == 0 || TileChunk->Tiles == 0) {
        return 0;
    }

    return GetTileChunkValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY);
}

internal void SetTileChunkValue(
    tile_map* TileMap,
    tile_chunk* TileChunk,
    uint32 TestTileX, uint32 TestTileY,
    uint32 TileValue
) {
    if (TileChunk == 0 || TileChunk->Tiles == 0) {
        return;
    }

    return SetTileChunkValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY, TileValue);
}

internal inline tile_chunk_position GetChunkPositionFor(tile_map* TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    tile_chunk_position Result;
    Result.TileChunkX = AbsTileX >> TileMap->ChunkShift;
    Result.TileChunkY = AbsTileY >> TileMap->ChunkShift;
    Result.TileChunkZ = AbsTileZ;
    Result.RelTileX = AbsTileX & TileMap->ChunkMask;
    Result.RelTileY = AbsTileY & TileMap->ChunkMask;
    return Result;
}

internal uint32 GetTileValue(tile_map* TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    tile_chunk* TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ);
    uint32 TileValue = GetTileChunkValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY);
    return TileValue;
}

internal uint32 GetTileValue(tile_map* TileMap, tile_map_position Pos) {
    return GetTileValue(TileMap, Pos.AbsTileX, Pos.AbsTileY, Pos.AbsTileZ);
}

internal bool32 IsTileMapPointEmpty(tile_map* TileMap, tile_map_position CanonicalPos) {
    uint32 TileValue = GetTileValue(TileMap, CanonicalPos.AbsTileX, CanonicalPos.AbsTileY, CanonicalPos.AbsTileZ);
    return TileValue == 1 || TileValue == 3 || TileValue == 4;
}

internal inline void RecanonicalizeCoord(tile_map* TileMap, uint32* Tile, real32* TileRel) {
    int32 Offset = RoundReal32ToInt32((*TileRel) / TileMap->TileSideInMeters);

    *Tile += Offset;
    *TileRel -= Offset * TileMap->TileSideInMeters;

    //! Sometimes it's right on
    Assert(*TileRel >= -0.5f * TileMap->TileSideInMeters);
    Assert(*TileRel <= 0.5f * TileMap->TileSideInMeters);
}

internal inline tile_map_position RecanonicalizePosition(tile_map* TileMap, tile_map_position Pos) {
    tile_map_position Result = Pos;

    RecanonicalizeCoord(TileMap, &Result.AbsTileX, &Result.Offset.X);
    RecanonicalizeCoord(TileMap, &Result.AbsTileY, &Result.Offset.Y);

    return Result;
}

internal void SetTileValue(
    memory_arena* Arena, tile_map* TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ, uint32 TileValue
) {
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    tile_chunk* TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ);
    Assert(TileChunk != 0);
    if (TileChunk->Tiles == 0) {
        uint32 TileCount = TileMap->ChunkDim * TileMap->ChunkDim;
        TileChunk->Tiles = PushArray(Arena, TileCount, uint32);
        for (uint32 TileIndex = 0; TileIndex < TileCount; ++TileIndex) {
            TileChunk->Tiles[TileIndex] = 1;
        }
    }
    SetTileChunkValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY, TileValue);
}

internal bool32 AreOnSameTile(tile_map_position* Pos1, tile_map_position* Pos2) {
    return Pos1->AbsTileX == Pos2->AbsTileX &&
        Pos1->AbsTileY == Pos2->AbsTileY &&
        Pos1->AbsTileZ == Pos2->AbsTileZ;
}

struct tile_map_difference {
    v2 dXY;
    real32 dZ;
};

tile_map_difference Subtract(tile_map* TileMap, tile_map_position* A, tile_map_position* B) {
    tile_map_difference Result = {};

    v2 dTileXY = { (real32)A->AbsTileX - (real32)B->AbsTileX, (real32)A->AbsTileY - (real32)B->AbsTileY };

    real32 dTileZ = TileMap->TileSideInMeters * ((real32)A->AbsTileZ - (real32)B->AbsTileZ);

    Result.dXY = TileMap->TileSideInMeters * dTileXY + (A->Offset - B->Offset);

    Result.dZ = dTileZ;

    return Result;
}

#endif
