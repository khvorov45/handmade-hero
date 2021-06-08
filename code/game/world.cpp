#if !defined(HANDMADE_WORLD_CPP)
#define HANDMADE_WORLD_CPP

#include "../intrinsics.H"
#include "../types.h"
#include "../util.h"
#include "memory.cpp"
#include "math.cpp"

struct world_position {
    int32 ChunkX;
    int32 ChunkY;
    int32 ChunkZ;

    //* Offset from the chunk center
    v2 Offset_;
};

struct world_difference {
    v3 d;
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
    int32 ChunkX;
    int32 ChunkY;
    int32 ChunkZ;

    int32 RelTileX;
    int32 RelTileY;
};
#endif

struct world {
    real32 TileSideInMeters;
    real32 ChunkSideInMeters;
    world_chunk ChunkHash[4096];
    world_entity_block* FirstFree;
};

#if 0
internal inline uint32 GetChunkValueUnchecked(
    world* World, world_chunk* Chunk, int32 TileX, int32 TileY
) {
    Assert(Chunk != 0);
    Assert(TileX < World->ChunkDim);
    Assert(TileY < World->ChunkDim);
    return Chunk->Tiles[TileY * World->ChunkDim + TileX];
}

internal inline void SetChunkValueUnchecked(
    world* World, world_chunk* Chunk, int32 TileX, int32 TileY, uint32 TileValue
) {
    Assert(Chunk != 0);
    Assert(TileX < World->ChunkDim);
    Assert(TileY < World->ChunkDim);
    Chunk->Tiles[TileY * World->ChunkDim + TileX] = TileValue;
}
internal uint32 GetChunkValue(
    world* World,
    world_chunk* Chunk,
    uint32 TestTileX, uint32 TestTileY
) {
    if (Chunk == 0 || Chunk->Tiles == 0) {
        return 0;
    }
    return GetChunkValueUnchecked(World, Chunk, TestTileX, TestTileY);
}
#endif

#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX / 64)
#define TILE_CHUNK_UNINIT INT32_MAX

internal inline world_position NullPosition() {
    world_position Result = {};
    Result.ChunkX = TILE_CHUNK_UNINIT;
    return Result;
}

internal inline bool32 IsValid(world_position Position) {
    return Position.ChunkX != TILE_CHUNK_UNINIT;
}

internal inline world_chunk* GetChunk(
    world* World, int32 ChunkX, int32 ChunkY, int32 ChunkZ,
    memory_arena* Arena = 0
) {
    Assert(ChunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkZ > -TILE_CHUNK_SAFE_MARGIN);

    Assert(ChunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkY < TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkZ < TILE_CHUNK_SAFE_MARGIN);

    uint32 HashValue = 19 * ChunkX + 7 * ChunkY + 3 * ChunkZ;
    uint32 HashSlot = HashValue & (ArrayCount(World->ChunkHash) - 1);
    Assert(HashSlot < ArrayCount(World->ChunkHash));

    world_chunk* Chunk = World->ChunkHash + HashSlot;

    while (Chunk != 0) {
        if ((ChunkX == Chunk->ChunkX) &&
            (ChunkY == Chunk->ChunkY) &&
            (ChunkZ == Chunk->ChunkZ)) {
            break;
        }

        if (Arena != 0 && Chunk->ChunkX != TILE_CHUNK_UNINIT && Chunk->NextInHash == 0) {
            Chunk->NextInHash = PushStruct(Arena, world_chunk);
            Chunk = Chunk->NextInHash;
            Chunk->ChunkX = TILE_CHUNK_UNINIT;
        }

        if (Arena != 0 && Chunk->ChunkX == TILE_CHUNK_UNINIT) {

            Chunk->ChunkX = ChunkX;
            Chunk->ChunkY = ChunkY;
            Chunk->ChunkZ = ChunkZ;

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
    world_chunk* Chunk,
    uint32 TestTileX, uint32 TestTileY,
    uint32 TileValue
) {
    if (Chunk == 0 || Chunk->Tiles == 0) {
        return;
    }

    return SetChunkValueUnchecked(World, Chunk, TestTileX, TestTileY, TileValue);
}

internal inline world_chunk_position GetChunkPositionFor(world* World, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_chunk_position Result;
    Result.ChunkX = AbsTileX >> World->ChunkShift;
    Result.ChunkY = AbsTileY >> World->ChunkShift;
    Result.ChunkZ = AbsTileZ;
    Result.RelTileX = AbsTileX & World->ChunkMask;
    Result.RelTileY = AbsTileY & World->ChunkMask;
    return Result;
}

internal uint32 GetTileValue(world* World, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_chunk_position ChunkPos = GetChunkPositionFor(World, AbsTileX, AbsTileY, AbsTileZ);
    world_chunk* Chunk = GetChunk(World, ChunkPos.ChunkX, ChunkPos.ChunkY, ChunkPos.ChunkZ);
    uint32 TileValue = GetChunkValue(World, Chunk, ChunkPos.RelTileX, ChunkPos.RelTileY);
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

internal bool32 IsCanonical(world* World, real32 TileRel) {
    //! Sometimes it's right on
    bool32 Result = (TileRel >= -0.5f * World->ChunkSideInMeters)
        && (TileRel <= 0.5f * World->ChunkSideInMeters);
    return Result;
}

internal bool32 IsCanonical(world* World, v2 Offset) {
    bool32 Result = IsCanonical(World, Offset.X) && IsCanonical(World, Offset.Y);
    return Result;
}

internal inline void RecanonicalizeCoord(world* World, int32* Tile, real32* TileRel) {
    int32 Offset = RoundReal32ToInt32((*TileRel) / World->ChunkSideInMeters);

    *Tile += Offset;
    *TileRel -= Offset * World->ChunkSideInMeters;

    Assert(IsCanonical(World, *TileRel));
}

internal inline world_position MapIntoChunkSpace(world* World, world_position BasePos, v2 Offset) {
    world_position Result = BasePos;

    Result.Offset_ += Offset;
    RecanonicalizeCoord(World, &Result.ChunkX, &Result.Offset_.X);
    RecanonicalizeCoord(World, &Result.ChunkY, &Result.Offset_.Y);

    return Result;
}

#if 0
internal void SetTileValue(
    memory_arena* Arena, world* World,
    uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ,
    uint32 TileValue
) {
    world_chunk_position ChunkPos = GetChunkPositionFor(World, AbsTileX, AbsTileY, AbsTileZ);
    world_chunk* Chunk =
        GetChunk(World, ChunkPos.ChunkX, ChunkPos.ChunkY, ChunkPos.ChunkZ, Arena);
    SetTileValue(World, Chunk, ChunkPos.RelTileX, ChunkPos.RelTileY, TileValue);
}
#endif

internal bool32 AreInSameChunk(world* World, world_position* Pos1, world_position* Pos2) {
    Assert(IsCanonical(World, Pos1->Offset_));
    Assert(IsCanonical(World, Pos2->Offset_));

    return Pos1->ChunkX == Pos2->ChunkX &&
        Pos1->ChunkY == Pos2->ChunkY &&
        Pos1->ChunkZ == Pos2->ChunkZ;
}

world_difference Subtract(world* World, world_position* A, world_position* B) {
    world_difference Result = {};

    v3 dTile = {
        World->ChunkSideInMeters * ((real32)A->ChunkX - (real32)B->ChunkX) + (A->Offset_.X - B->Offset_.X),
        World->ChunkSideInMeters * ((real32)A->ChunkY - (real32)B->ChunkY) + (A->Offset_.Y - B->Offset_.Y),
        World->ChunkSideInMeters * ((real32)A->ChunkZ - (real32)B->ChunkZ)
    };

    Result.d = dTile;

    return Result;
}

internal world_position CenteredChunkPoint(uint32 ChunkX, uint32 ChunkY, uint32 ChunkZ) {
    world_position Pos;
    Pos.ChunkX = ChunkX;
    Pos.ChunkY = ChunkY;
    Pos.ChunkZ = ChunkZ;
    Pos.Offset_ = { 0.0f, 0.0f };
    return Pos;
}

#define TILES_PER_CHUNK 16

internal void InitializeWorld(world* World, real32 TileSideInMeters) {

    World->TileSideInMeters = TileSideInMeters;
    World->ChunkSideInMeters = (real32)TILES_PER_CHUNK * TileSideInMeters;
    World->FirstFree = 0;

    for (uint32 ChunkIndex = 0; ChunkIndex < ArrayCount(World->ChunkHash); ++ChunkIndex) {
        World->ChunkHash[ChunkIndex].ChunkX = TILE_CHUNK_UNINIT;
        World->ChunkHash[ChunkIndex].FirstBlock.EntityCount = 0;
    }
}

internal void ChangeEntityLocationRaw(
    memory_arena* Arena,
    world* World, uint32 LowEntityIndex,
    world_position* OldP, world_position* NewP
) {
    Assert(OldP == 0 || IsValid(*OldP));
    Assert(NewP == 0 || IsValid(*NewP));

    if (OldP && AreInSameChunk(World, OldP, NewP)) {
        return;
    }
    if (OldP) {
        world_chunk* Chunk = GetChunk(World, OldP->ChunkX, OldP->ChunkY, OldP->ChunkZ);
        Assert(Chunk);
        world_entity_block* FirstBlock = &Chunk->FirstBlock;
        bool32 Found = false;
        for (world_entity_block* Block = FirstBlock; Block && !Found; Block = Block->Next) {
            for (uint32 Index = 0; Index < Block->EntityCount && !Found; ++Index) {
                if (Block->LowEntityIndex[Index] == LowEntityIndex) {
                    Assert(FirstBlock->EntityCount > 0);
                    Block->LowEntityIndex[Index] =
                        FirstBlock->LowEntityIndex[--FirstBlock->EntityCount];
                    if (FirstBlock->EntityCount == 0) {
                        if (FirstBlock->Next) {
                            world_entity_block* NextBlock = FirstBlock->Next;
                            *FirstBlock = *NextBlock;

                            NextBlock->Next = World->FirstFree;
                            World->FirstFree = NextBlock;
                        }
                    }
                    Found = true;
                }
            }
        }
    }
    if (NewP) {
        world_chunk* Chunk = GetChunk(World, NewP->ChunkX, NewP->ChunkY, NewP->ChunkZ, Arena);
        Assert(Chunk);
        world_entity_block* Block = &Chunk->FirstBlock;
        if (Block->EntityCount == ArrayCount(Block->LowEntityIndex)) {
            world_entity_block* OldBlock = World->FirstFree;
            if (OldBlock) {
                World->FirstFree = OldBlock->Next;
            } else {
                OldBlock = PushStruct(Arena, world_entity_block);
            }
            *OldBlock = *Block;
            Block->Next = OldBlock;
            Block->EntityCount = 0;
        }
        Assert(Block->EntityCount < ArrayCount(Block->LowEntityIndex));
        Block->LowEntityIndex[Block->EntityCount++] = LowEntityIndex;
    }
}

inline world_position ChunkPositionFromTilePosition(
    world* World,
    int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ
) {
    world_position Result = {};

    Result.ChunkX = AbsTileX / TILES_PER_CHUNK;
    Result.ChunkY = AbsTileY / TILES_PER_CHUNK;
    Result.ChunkZ = AbsTileZ / TILES_PER_CHUNK;

    if (AbsTileX < 0) {
        Result.ChunkX--;
    }
    if (AbsTileY < 0) {
        Result.ChunkY--;
    }
    if (AbsTileZ < 0) {
        Result.ChunkZ--;
    }

    Result.Offset_.X =
        (real32)((AbsTileX - TILES_PER_CHUNK / 2) - Result.ChunkX * TILES_PER_CHUNK) * World->TileSideInMeters;
    Result.Offset_.Y =
        (real32)((AbsTileY - TILES_PER_CHUNK / 2) - Result.ChunkY * TILES_PER_CHUNK) * World->TileSideInMeters;

    Assert(IsCanonical(World, Result.Offset_));

    return Result;
}

#endif
