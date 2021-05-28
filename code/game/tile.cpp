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
    uint32 AbsTileX;
    uint32 AbsTileY;
    uint32 AbsTileZ;

    //* Offset from the center
    v2 Offset_;
};

struct tile_chunk {
    uint32 TileChunkX;
    uint32 TileChunkY;
    uint32 TileChunkZ;

    uint32* Tiles;

    tile_chunk* NextInHash;
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

    tile_chunk TileChunkHash[4096];
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

internal inline tile_chunk* GetTileChunk(
    tile_map* TileMap, uint32 TileChunkX, uint32 TileChunkY, uint32 TileChunkZ,
    memory_arena* Arena = 0
) {

#define TILE_CHUNK_SAFE_MARGIN 256

    Assert(TileChunkX > TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkY > TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkZ > TILE_CHUNK_SAFE_MARGIN);

    Assert(TileChunkX < UINT32_MAX - TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkY < UINT32_MAX - TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkZ < UINT32_MAX - TILE_CHUNK_SAFE_MARGIN);

    uint32 HashValue = 19 * TileChunkX + 7 * TileChunkY + 3 * TileChunkZ;
    uint32 HashSlot = HashValue & (ArrayCount(TileMap->TileChunkHash) - 1);
    Assert(HashSlot < ArrayCount(TileMap->TileChunkHash));

    tile_chunk* Chunk = TileMap->TileChunkHash + HashSlot;

    while (Chunk != 0) {
        if ((TileChunkX == Chunk->TileChunkX) &&
            (TileChunkY == Chunk->TileChunkY) &&
            (TileChunkZ == Chunk->TileChunkY)) {
            break;
        }

        if (Arena != 0 && Chunk->Tiles != 0 && Chunk->NextInHash == 0) {
            Chunk->NextInHash = PushStruct(Arena, tile_chunk);
            Chunk = Chunk->NextInHash;
            Chunk->Tiles = 0;
        }

        if (Arena != 0 && Chunk->Tiles == 0) {
            uint32 TileCount = TileMap->ChunkDim * TileMap->ChunkDim;

            Chunk->TileChunkX = TileChunkX;
            Chunk->TileChunkY = TileChunkY;
            Chunk->TileChunkZ = TileChunkZ;

            Chunk->Tiles = PushArray(Arena, TileCount, uint32);
            for (uint32 TileIndex = 0; TileIndex < TileCount; ++TileIndex) {
                Chunk->Tiles[TileIndex] = 1;
            }
            Chunk->NextInHash = 0;
            break;
        }
        Chunk = Chunk->NextInHash;
    }

    return Chunk;
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

internal void SetTileValue(
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

internal bool32 IsTileValueEmpty(uint32 TileValue) {
    return TileValue == 1 || TileValue == 3 || TileValue == 4;
}

internal bool32 IsTileMapPointEmpty(tile_map* TileMap, tile_map_position CanonicalPos) {
    uint32 TileValue = GetTileValue(TileMap, CanonicalPos.AbsTileX, CanonicalPos.AbsTileY, CanonicalPos.AbsTileZ);
    return IsTileValueEmpty(TileValue);
}

internal inline void RecanonicalizeCoord(tile_map* TileMap, uint32* Tile, real32* TileRel) {
    int32 Offset = RoundReal32ToInt32((*TileRel) / TileMap->TileSideInMeters);

    *Tile += Offset;
    *TileRel -= Offset * TileMap->TileSideInMeters;

    //! Sometimes it's right on
    Assert(*TileRel > -0.5f * TileMap->TileSideInMeters);
    Assert(*TileRel < 0.5f * TileMap->TileSideInMeters);
}

internal inline tile_map_position MapIntoTileSpace(tile_map* TileMap, tile_map_position BasePos, v2 Offset) {
    tile_map_position Result = BasePos;

    Result.Offset_ += Offset;
    RecanonicalizeCoord(TileMap, &Result.AbsTileX, &Result.Offset_.X);
    RecanonicalizeCoord(TileMap, &Result.AbsTileY, &Result.Offset_.Y);

    return Result;
}

internal void SetTileValue(
    memory_arena* Arena, tile_map* TileMap,
    uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ,
    uint32 TileValue
) {
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    tile_chunk* TileChunk =
        GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ, Arena);
    SetTileValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY, TileValue);
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

internal void InitializeTileMap(tile_map* TileMap, real32 TileSideInMeters) {
    TileMap->ChunkShift = 4;
    TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
    TileMap->ChunkDim = (1 << TileMap->ChunkShift);

    TileMap->TileSideInMeters = TileSideInMeters;

    for (uint32 TileChunkIndex = 0; TileChunkIndex < ArrayCount(TileMap->TileChunkHash); ++TileChunkIndex) {
        TileMap->TileChunkHash[TileChunkIndex].Tiles = 0;
    }
}

#endif
