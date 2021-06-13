#if !defined(HANDMADE_SIM_REGION_CPP)
#define HANDMADE_SIM_REGION_CPP

#include "../types.h"
#include "state.cpp"
#include "world.cpp"

struct sim_entity;
union entity_reference {
    sim_entity* Ptr;
    uint32 Index;
};

struct sim_entity {
    uint32 StorageIndex;

    entity_type Type;

    v2 P;
    uint32 ChunkZ;

    real32 Z;
    real32 dZ;

    v2 dP;
    real32 Width;
    real32 Height;

    uint32 FacingDirection;
    real32 tBob;

    bool32 Collides;
    int32 dAbsTileZ; //* Stairs

    uint32 HitPointMax;
    hit_point HitPoint[16];

    entity_reference Sword;
    real32 DistanceRemaining;
};

struct low_entity_ {
    world_position P;
    sim_entity Sim;
};

struct sim_entity_hash {
    sim_entity* Ptr;
    uint32 Index;
};

struct sim_region {
    world* World;

    world_position Origin;
    rectangle2 Bounds;

    uint32 MaxEntityCount;
    uint32 EntityCount;
    sim_entity* Entities;

    sim_entity_hash Hash[4096];
};

struct game_state {
    memory_arena WorldArena;
    world* World;
    uint32 CameraFollowingEntityIndex;
    world_position CameraP;

    controlled_hero ControlledHeroes[ArrayCount(((game_input*)0)->Controllers)];

    uint32 HighEntityCount;
    high_entity HighEntities_[256];

    uint32 LowEntityCount;
    low_entity LowEntities_[1000000];

    uint32 LowEntity_Count;
    low_entity_ LowEntities[1000000];

    loaded_bitmap Backdrop;
    hero_bitmaps HeroBitmaps[4];
    loaded_bitmap HeroShadow;

    loaded_bitmap Tree;
    loaded_bitmap Sword;

    real32 MetersToPixels;
};

struct entity_visible_piece_group {
    game_state* GameState;
    uint32 PieceCount;
    entity_visible_piece Pieces[32];
};

internal v2 GetSimSpaceP(sim_region* SimRegion, low_entity_* Stored) {
    world_difference Diff = Subtract(SimRegion->World, &Stored->P, &SimRegion->Origin);
    return { Diff.d.X, Diff.d.Y };
}

internal sim_entity_hash* GetHashFromStorageIndex(sim_region* SimRegion, uint32 StorageIndex) {
    Assert(StorageIndex);
    sim_entity_hash* Result = 0;
    uint32 HashValue = StorageIndex;
    for (uint32 Offset = 0; Offset < ArrayCount(SimRegion->Hash); ++Offset) {
        sim_entity_hash* Entry =
            SimRegion->Hash + ((HashValue + Offset) & (ArrayCount(SimRegion->Hash) - 1));
        if (Entry->Index == StorageIndex || Entry->Index == 0) {
            Result = Entry;
            break;
        }
    }
    return Result;
}

internal low_entity_* GetLowEntity_(game_state* GameState, uint32 LowIndex) {

    low_entity_* Result = 0;

    if (LowIndex > 0 && LowIndex < GameState->LowEntityCount) {
        Result = GameState->LowEntities + LowIndex;
    }

    return Result;
}

internal sim_entity* GetEntityByStorageIndex(sim_region* SimRegion, uint32 StorageIndex) {
    sim_entity_hash* Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
    return Entry->Ptr;
}

internal sim_entity*
AddEntity(game_state* GameState, sim_region* SimRegion, uint32 StorageIndex, low_entity_* Source);

internal void
LoadEntityReference(game_state* GameState, sim_region* SimRegion, entity_reference* Ref) {
    if (Ref->Index) {
        sim_entity_hash* Entry = GetHashFromStorageIndex(SimRegion, Ref->Index);
        if (Entry->Ptr == 0) {
            Entry->Index = Ref->Index;
            Entry->Ptr =
                AddEntity(GameState, SimRegion, Ref->Index, GetLowEntity_(GameState, Ref->Index));
        }
        Ref->Ptr = Entry->Ptr;
    }
}

internal void
MapStorageIndexToEntity(sim_region* SimRegion, uint32 StorageIndex, sim_entity* Entity) {
    sim_entity_hash* Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
    Assert(Entry->Index == 0 || Entry->Index == StorageIndex);
    Entry->Index = StorageIndex;
    Entry->Ptr = Entity;
}

internal sim_entity*
AddEntity(game_state* GameState, sim_region* SimRegion, uint32 StorageIndex, low_entity_* Source) {
    Assert(StorageIndex);
    sim_entity* Entity = 0;

    if (SimRegion->EntityCount < SimRegion->MaxEntityCount) {

        Entity = SimRegion->Entities + SimRegion->EntityCount++;
        MapStorageIndexToEntity(SimRegion, StorageIndex, Entity);

        if (Source) {
            *Entity = Source->Sim;
            LoadEntityReference(GameState, SimRegion, &Entity->Sword);
        }
        Entity->StorageIndex = StorageIndex;

    } else {
        InvalidCodePath;
    }

    return Entity;
}

internal sim_entity*
AddEntity(
    game_state* GameState, sim_region* SimRegion, uint32 LowEntityIndex,
    low_entity_* Source, v2* SimP
) {
    sim_entity* Dest = AddEntity(GameState, SimRegion, LowEntityIndex, Source);
    if (Dest) {
        if (SimP) {
            Dest->P = *SimP;
        } else {
            Dest->P = GetSimSpaceP(SimRegion, Source);
        }
    }
    return Dest;
}

internal sim_region*
BeginSim(
    memory_arena* SimArena, game_state* GameState,
    world* World, world_position Origin, rectangle2 Bounds
) {
    sim_region* SimRegion = PushStruct(SimArena, sim_region);

    SimRegion->World = World;

    SimRegion->Origin = Origin;
    SimRegion->Bounds = Bounds;

    SimRegion->MaxEntityCount = 4096;
    SimRegion->EntityCount = 0;
    SimRegion->Entities = PushArray(SimArena, SimRegion->MaxEntityCount, sim_entity);

    world_position MinChunkP =
        MapIntoChunkSpace(World, Origin, GetMinCorner(Bounds));
    world_position MaxChunkP =
        MapIntoChunkSpace(World, Origin, GetMaxCorner(Bounds));

    for (int32 ChunkY = MinChunkP.ChunkY; ChunkY <= MaxChunkP.ChunkY; ChunkY++) {

        for (int32 ChunkX = MinChunkP.ChunkX; ChunkX <= MaxChunkP.ChunkX; ChunkX++) {

            world_chunk* Chunk = GetChunk(World, ChunkX, ChunkY, Origin.ChunkZ);
            if (Chunk == 0) {
                continue;
            }

            for (world_entity_block* Block = &Chunk->FirstBlock; Block; Block = Block->Next) {

                for (uint32 BlockIndex = 0; BlockIndex < Block->EntityCount; ++BlockIndex) {

                    uint32 LowEntityIndex = Block->LowEntityIndex[BlockIndex];
                    low_entity_* Low = GameState->LowEntities + LowEntityIndex;

                    v2 SimSpaceP = GetSimSpaceP(SimRegion, Low);
                    if (IsInRectangle(Bounds, SimSpaceP)) {
                        AddEntity(GameState, SimRegion, LowEntityIndex, Low, &SimSpaceP);
                    }

                }
            }
        }
    }
    return SimRegion;
}

internal void StoreEntityReference(entity_reference* Ref) {
    if (Ref->Ptr != 0) {
        Ref->Index = Ref->Ptr->StorageIndex;
    }
}

internal void ChangeEntityLocation(
    memory_arena* Arena,
    world* World, uint32 LowEntityIndex, low_entity_* EntityLow,
    world_position* OldP, world_position* NewP
) {
    ChangeEntityLocationRaw(Arena, World, LowEntityIndex, OldP, NewP);
    if (NewP) {
        EntityLow->P = *NewP;
    } else {
        EntityLow->P = NullPosition();
    }
}

internal void EndSim(sim_region* Region, game_state* GameState) {

    sim_entity* Entity = Region->Entities;

    for (uint32 EntityIndex = 0;
        EntityIndex < Region->EntityCount;
        EntityIndex++, Entity++) {

        //! stored_entity
        low_entity_* Stored = GameState->LowEntities + Entity->StorageIndex;
        Stored->Sim = *Entity;
        StoreEntityReference(&Stored->Sim.Sword);

        world_position NewP = MapIntoChunkSpace(GameState->World, Region->Origin, Entity->P);

        ChangeEntityLocation(
            &GameState->WorldArena, GameState->World, Entity->StorageIndex, Stored,
            &Stored->P, &NewP
        );

        if (Entity->StorageIndex == GameState->CameraFollowingEntityIndex) {

            world_position NewCameraP = GameState->CameraP;

            NewCameraP.ChunkZ = Stored->P.ChunkZ;

#if 0
            if (CameraFollowingEntity.High->P.X > 9.0f * TileMap->TileSideInMeters) {
                NewCameraP.AbsTileX += 17;
            } else if (CameraFollowingEntity.High->P.X < -9.0f * TileMap->TileSideInMeters) {
                NewCameraP.AbsTileX -= 17;
            }
            if (CameraFollowingEntity.High->P.Y > 5.0f * TileMap->TileSideInMeters) {
                NewCameraP.AbsTileY += 9;
            } else if (CameraFollowingEntity.High->P.Y < -5.0f * TileMap->TileSideInMeters) {
                NewCameraP.AbsTileY -= 9;
            }
#else
            NewCameraP = Stored->P;
#endif
            GameState->CameraP = NewCameraP;
        }
    }
}

internal bool32 TestWall(
    real32 WallX, real32 RelX, real32 RelY, real32 PlayerDeltaX, real32 PlayerDeltaY,
    real32* tMin, real32 MinY, real32 MaxY
) {
    bool32 Hit = false;
    if (PlayerDeltaX != 0.0f) {
        real32 tResult = (WallX - RelX) / PlayerDeltaX;
        real32 Y = RelY + tResult * PlayerDeltaY;
        if (tResult > 0.0f && *tMin > tResult) {
            if (Y >= MinY && Y <= MaxY) {
                Hit = true;
                real32 tEpsilon = 0.0001f;
                *tMin = Maximum(0.0f, tResult - tEpsilon);
            }
        }
    }
    return Hit;
}

struct move_spec {
    bool32 UnitMaxAccelVector;
    real32 Speed;
    real32 Drag;
};

internal inline move_spec DefaultMoveSpec() {
    move_spec MoveSpec = {};
    MoveSpec.Drag = 0.0f;
    MoveSpec.Speed = 1.0f;
    MoveSpec.UnitMaxAccelVector = false;
    return MoveSpec;
}

internal void
MoveEntity(sim_region* Region, sim_entity* Entity, real32 dt, move_spec* MoveSpec, v2 ddPlayer) {

    world* World = Region->World;

    if (MoveSpec->UnitMaxAccelVector) {
        real32 ddPlayerLengthSq = LengthSq(ddPlayer);
        if (ddPlayerLengthSq > 1.0f) {
            ddPlayer *= 1.0f / SquareRoot(ddPlayerLengthSq);
        }
    }

    ddPlayer *= MoveSpec->Speed;

    ddPlayer += -MoveSpec->Drag * Entity->dP;

    v2 OldPlayerP = Entity->P;

    v2 PlayerDelta = 0.5f * ddPlayer * Square(dt) + Entity->dP * dt;

    v2 NewPlayerP = OldPlayerP + PlayerDelta;
    Entity->dP += ddPlayer * dt;

    for (uint32 Iteration = 0; Iteration < 4; ++Iteration) {

        real32 tMin = 1.0f;
        v2 WallNormal = {};
        sim_entity* HitEntity = 0;

        v2 DesiredPosition = Entity->P + PlayerDelta;

        if (Entity->Collides) {

            for (uint32 TestHighEntityIndex = 1;
                TestHighEntityIndex < Region->EntityCount;
                ++TestHighEntityIndex) {

                sim_entity* TestEntity = Region->Entities + TestHighEntityIndex;
                if (TestEntity == Entity || !TestEntity->Collides) {
                    continue;
                }

                real32 DiameterW = Entity->Width + TestEntity->Width;
                real32 DiameterH = Entity->Height + TestEntity->Height;

                v2 MinCorner = -0.5f * v2{ DiameterW, DiameterH };
                v2 MaxCorner = 0.5f * v2{ DiameterW, DiameterH };

                v2 Rel = Entity->P - TestEntity->P;

                if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y)) {
                    WallNormal = { -1, 0 };
                    HitEntity = TestEntity;
                }
                if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y)) {
                    WallNormal = { 1, 0 };
                    HitEntity = TestEntity;
                }
                if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X)) {
                    WallNormal = { 0, -1 };
                    HitEntity = TestEntity;
                }
                if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X)) {
                    WallNormal = { 0, 1 };
                    HitEntity = TestEntity;
                }
            }
        }

        Entity->P += tMin * PlayerDelta;
        if (HitEntity != 0) {
            Entity->dP = Entity->dP - Inner(Entity->dP, WallNormal) * WallNormal;
            PlayerDelta = DesiredPosition - Entity->P;
            PlayerDelta = PlayerDelta - Inner(PlayerDelta, WallNormal) * WallNormal;

            //HitHigh->AbsTileZ += HitLow->dAbsTileZ;
        } else {
            break;
        }

    }

    if (AbsoluteValue(Entity->dP.Y) > AbsoluteValue(Entity->dP.X)) {
        if (Entity->dP.Y > 0) {
            Entity->FacingDirection = 1;
        } else {
            Entity->FacingDirection = 3;
        }
    } else if (AbsoluteValue(Entity->dP.Y) < AbsoluteValue(Entity->dP.X)) {
        if (Entity->dP.X > 0) {
            Entity->FacingDirection = 0;
        } else {
            Entity->FacingDirection = 2;
        }
    }
}

#endif
