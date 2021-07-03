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
    v3 Offset_;
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

struct world {
    real32 TileSideInMeters;
    real32 TileDepthInMeters;

    v3 ChunkDimInMeters;

    world_chunk ChunkHash[4096];
    world_entity_block* FirstFree;
};

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

internal bool32 IsTileValueEmpty(uint32 TileValue) {
    return TileValue == 1 || TileValue == 3 || TileValue == 4;
}

internal bool32 IsCanonical(real32 ChunkDim, real32 TileRel) {
    //! Sometimes it's right on
    real32 Epsilon = 0.01f;
    bool32 Result = (TileRel >= -(0.5f * ChunkDim + Epsilon))
        && (TileRel <= 0.5f * ChunkDim + Epsilon);
    return Result;
}

internal bool32 IsCanonical(world* World, v3 Offset) {
    bool32 Result =
        IsCanonical(World->ChunkDimInMeters.X, Offset.X) &&
        IsCanonical(World->ChunkDimInMeters.Y, Offset.Y) &&
        IsCanonical(World->ChunkDimInMeters.Z, Offset.Z);
    return Result;
}

internal inline void RecanonicalizeCoord(real32 ChunkDim, int32* Tile, real32* TileRel) {
    int32 Offset = RoundReal32ToInt32((*TileRel) / ChunkDim);

    *Tile += Offset;
    *TileRel -= Offset * ChunkDim;

    Assert(IsCanonical(ChunkDim, *TileRel));
}

internal inline world_position MapIntoChunkSpace(world* World, world_position BasePos, v3 Offset) {
    world_position Result = BasePos;

    Result.Offset_ += Offset;
    RecanonicalizeCoord(World->ChunkDimInMeters.X, &Result.ChunkX, &Result.Offset_.X);
    RecanonicalizeCoord(World->ChunkDimInMeters.Y, &Result.ChunkY, &Result.Offset_.Y);
    RecanonicalizeCoord(World->ChunkDimInMeters.Z, &Result.ChunkZ, &Result.Offset_.Z);

    return Result;
}

internal bool32 AreInSameChunk(world* World, world_position* Pos1, world_position* Pos2) {
    Assert(IsCanonical(World, Pos1->Offset_));
    Assert(IsCanonical(World, Pos2->Offset_));

    return Pos1->ChunkX == Pos2->ChunkX &&
        Pos1->ChunkY == Pos2->ChunkY &&
        Pos1->ChunkZ == Pos2->ChunkZ;
}

v3 Subtract(world* World, world_position* A, world_position* B) {

    v3 Result = V3(
        ((real32)A->ChunkX - (real32)B->ChunkX),
        ((real32)A->ChunkY - (real32)B->ChunkY),
        ((real32)A->ChunkZ - (real32)B->ChunkZ)
    );

    Result = Hadamard(Result, World->ChunkDimInMeters) + (A->Offset_ - B->Offset_);

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

internal void InitializeWorld(world* World, real32 TileSideInMeters, real32 TileDepthInMeters) {

    World->TileSideInMeters = TileSideInMeters;
    World->TileDepthInMeters = TileDepthInMeters;
    World->ChunkDimInMeters = {
        (real32)TILES_PER_CHUNK * TileSideInMeters,
        (real32)TILES_PER_CHUNK * TileSideInMeters,
        (real32)TileDepthInMeters,
    };
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

    if (OldP && NewP && AreInSameChunk(World, OldP, NewP)) {
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
    int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ,
    v3 AdditionalOffset = {}
) {

    world_position BasePos = {};

    v3 Offset = Hadamard(
        V3(World->TileSideInMeters, World->TileSideInMeters, World->TileDepthInMeters),
        V3((real32)AbsTileX, (real32)AbsTileY, (real32)AbsTileZ)
    );

    world_position Result = MapIntoChunkSpace(World, BasePos, Offset + AdditionalOffset);

    Assert(IsCanonical(World, Result.Offset_));

    return Result;
}

#endif
