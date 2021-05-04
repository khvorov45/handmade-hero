#if !defined(HANDMADE_TILE_CPP)
#define HANDMADE_TILE_CPP

#include "../types.h"
#include "lib.hpp"

struct tile_map_position {
    //* High bits - tile chunk index
    //* Low bits - tile location in the chunk
    uint32 AbsTileX;
    uint32 AbsTileY;

    //* Offset from the center
    real32 XTileRel;
    real32 YTileRel;
};

struct tile_chunk {
    uint32* Tiles;
};

struct tile_chunk_position {
    uint32 TileChunkX;
    uint32 TileChunkY;

    uint32 RelTileX;
    uint32 RelTileY;
};

struct tile_map {
    uint32 ChunkShift;
    uint32 ChunkMask;
    uint32 ChunkDim;

    real32 TileSideInMeters;
    int32 TileSideInPixels;

    real32 MetersToPixels;

    int32 TileChunkCountX;
    int32 TileChunkCountY;

    tile_chunk* TileChunks;
};

internal inline uint32 GetTileChunkValueUnchecked(tile_map* TileMap, tile_chunk* TileChunk, uint32 TileX, uint32 TileY) {
    Assert(TileChunk != 0);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);
    return TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX];
}

internal inline tile_chunk* GetTileChunk(tile_map* TileMap, int32 TileChunkX, int32 TileChunkY) {
    tile_chunk* TileChunk = 0;
    if ((TileChunkX >= 0) && (TileChunkX < TileMap->TileChunkCountX) &&
        (TileChunkY >= 0) && (TileChunkY < TileMap->TileChunkCountY)) {
        TileChunk = &TileMap->TileChunks[TileChunkY * TileMap->TileChunkCountX + TileChunkX];
    }
    return TileChunk;
}

internal uint32 GetTileChunkValue(
    tile_map* TileMap,
    tile_chunk* TileChunk,
    uint32 TestTileX, uint32 TestTileY
) {
    if (TileChunk == 0) {
        return 0;
    }

    return GetTileChunkValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY);
}

internal inline tile_chunk_position GetChunkPositionFor(tile_map* TileMap, uint32 AbsTileX, uint32 AbsTileY) {
    tile_chunk_position Result;
    Result.TileChunkX = AbsTileX >> TileMap->ChunkShift;
    Result.TileChunkY = AbsTileY >> TileMap->ChunkShift;
    Result.RelTileX = AbsTileX & TileMap->ChunkMask;
    Result.RelTileY = AbsTileY & TileMap->ChunkMask;
    return Result;
}

internal uint32 GetTileValue(tile_map* TileMap, uint32 AbsTileX, uint32 AbsTileY) {
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY);
    tile_chunk* TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY);
    uint32 TileValue = GetTileChunkValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY);
    return TileValue;
}

internal bool32 IsTileMapPointEmpty(tile_map* TileMap, tile_map_position CanonicalPos) {
    uint32 TileValue = GetTileValue(TileMap, CanonicalPos.AbsTileX, CanonicalPos.AbsTileY);
    return TileValue == 0;
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

    RecanonicalizeCoord(TileMap, &Result.AbsTileX, &Result.XTileRel);
    RecanonicalizeCoord(TileMap, &Result.AbsTileY, &Result.YTileRel);

    return Result;
}

#endif
