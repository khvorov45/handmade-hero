#if !defined(HANDMADE_SIM_REGION_CPP)
#define HANDMADE_SIM_REGION_CPP

#include "../types.h"
#include "state.cpp"
#include "world.cpp"
#include "math.cpp"

struct sim_entity;
union entity_reference {
    sim_entity* Ptr;
    uint32 Index;
};

enum sim_entity_flags {
    EntityFlag_Collides = (1 << 1),
    EntityFlag_Nonspatial = (1 << 2),

    EntityFlag_Simming = (1 << 30)
};

struct sim_entity {
    uint32 StorageIndex;
    bool32 Updatable;

    entity_type Type;
    uint32 Flags;

    v3 P;
    v3 dP;

    real32 DistanceLimit;

    real32 Width;
    real32 Height;

    uint32 FacingDirection;
    real32 tBob;

    int32 dAbsTileZ; //* Stairs

    uint32 HitPointMax;
    hit_point HitPoint[16];

    entity_reference Sword;
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
    rectangle3 Bounds;
    rectangle3 UpdatableBounds;

    uint32 MaxEntityCount;
    uint32 EntityCount;
    sim_entity* Entities;

    sim_entity_hash Hash[4096];
};

struct pairwise_collision_rule {
    bool32 ShouldCollide;
    uint32 StorageIndexA;
    uint32 StorageIndexB;

    pairwise_collision_rule* NextInHash;
};

struct game_state {
    memory_arena WorldArena;
    world* World;
    uint32 CameraFollowingEntityIndex;
    world_position CameraP;

    controlled_hero ControlledHeroes[ArrayCount(((game_input*)0)->Controllers)];

    uint32 LowEntity_Count;
    low_entity_ LowEntities[1000000];

    loaded_bitmap Backdrop;
    hero_bitmaps HeroBitmaps[4];
    loaded_bitmap HeroShadow;

    loaded_bitmap Tree;
    loaded_bitmap Sword;

    real32 MetersToPixels;

    pairwise_collision_rule* CollisionRuleHash[256];
    pairwise_collision_rule* FirstFreeCollisionRule;
};

struct entity_visible_piece_group {
    game_state* GameState;
    uint32 PieceCount;
    entity_visible_piece Pieces[32];
};

internal bool32 IsSet(sim_entity* Entity, uint32 Flag) {
    bool32 Result = (Entity->Flags & Flag) != 0;
    return Result;
}

internal void AddFlag(sim_entity* Entity, uint32 Flag) {
    Entity->Flags |= Flag;
}

internal void ClearFlag(sim_entity* Entity, uint32 Flag) {
    Entity->Flags &= ~Flag;
}

#define InvalidP { 100000.0f, 100000.0f, 100000.0f }

internal void MakeEntityNonSpatial(sim_entity* Entity) {
    AddFlag(Entity, EntityFlag_Nonspatial);
    Entity->P = InvalidP;
}

internal void MakeEntitySpatial(sim_entity* Entity, v3 P, v3 dP) {
    ClearFlag(Entity, EntityFlag_Nonspatial);
    Entity->P = P;
    Entity->dP = dP;
}

internal v3 GetSimSpaceP(sim_region* SimRegion, low_entity_* Stored) {
    v3 Result = InvalidP;
    if (!IsSet(&Stored->Sim, EntityFlag_Nonspatial)) {
        Result = Subtract(SimRegion->World, &Stored->P, &SimRegion->Origin);
    }
    return Result;
}

internal sim_entity_hash* GetHashFromStorageIndex(sim_region* SimRegion, uint32 StorageIndex) {
    Assert(StorageIndex);
    sim_entity_hash* Result = 0;
    uint32 HashValue = StorageIndex;
    for (uint32 Offset = 0; Offset < ArrayCount(SimRegion->Hash); ++Offset) {
        uint32 HashMask = ArrayCount(SimRegion->Hash) - 1;
        uint32 HashIndex = (HashValue + Offset) & HashMask;
        sim_entity_hash* Entry = SimRegion->Hash + HashIndex;
        if (Entry->Index == StorageIndex || Entry->Index == 0) {
            Result = Entry;
            break;
        }
    }
    return Result;
}

internal low_entity_* GetLowEntity_(game_state* GameState, uint32 LowIndex) {

    low_entity_* Result = 0;

    if (LowIndex > 0 && LowIndex < GameState->LowEntity_Count) {
        Result = GameState->LowEntities + LowIndex;
    }

    return Result;
}

internal sim_entity* GetEntityByStorageIndex(sim_region* SimRegion, uint32 StorageIndex) {
    sim_entity_hash* Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
    return Entry->Ptr;
}

internal sim_entity*
AddEntity(
    game_state* GameState, sim_region* SimRegion, uint32 LowEntityIndex,
    low_entity_* Source, v3* SimP
);

internal void
LoadEntityReference(game_state* GameState, sim_region* SimRegion, entity_reference* Ref) {
    if (Ref->Index) {
        sim_entity_hash* Entry = GetHashFromStorageIndex(SimRegion, Ref->Index);
        if (Entry->Ptr == 0) {
            Entry->Index = Ref->Index;
            low_entity_* LowEntity = GetLowEntity_(GameState, Ref->Index);
            v3 EntityP = GetSimSpaceP(SimRegion, LowEntity);
            Entry->Ptr = AddEntity(GameState, SimRegion, Ref->Index, LowEntity, &EntityP);
        }
        Ref->Ptr = Entry->Ptr;
    }
}

internal sim_entity*
AddEntityRaw(
    game_state* GameState, sim_region* SimRegion, uint32 StorageIndex, low_entity_* Source
) {
    Assert(StorageIndex);
    sim_entity* Entity = 0;

    sim_entity_hash* Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);

    if (Entry->Ptr == 0) {
        if (SimRegion->EntityCount < SimRegion->MaxEntityCount) {

            Entity = SimRegion->Entities + SimRegion->EntityCount++;

            Entry->Index = StorageIndex;
            Entry->Ptr = Entity;

            if (Source) {
                *Entity = Source->Sim;
                LoadEntityReference(GameState, SimRegion, &Entity->Sword);

                Assert(!IsSet(&Source->Sim, EntityFlag_Simming));
                AddFlag(&Source->Sim, EntityFlag_Simming);
            }
            Entity->StorageIndex = StorageIndex;
            Entity->Updatable = false;

        } else {
            InvalidCodePath;
        }
    }

    return Entity;
}

internal sim_entity*
AddEntity(
    game_state* GameState, sim_region* SimRegion, uint32 LowEntityIndex,
    low_entity_* Source, v3* SimP
) {
    sim_entity* Dest = AddEntityRaw(GameState, SimRegion, LowEntityIndex, Source);
    if (Dest) {
        if (SimP) {
            Dest->P = *SimP;
            Dest->Updatable = IsInRectangle(SimRegion->UpdatableBounds, Dest->P);
        } else {
            Dest->P = GetSimSpaceP(SimRegion, Source);
        }
    }
    return Dest;
}

internal sim_region*
BeginSim(
    memory_arena* SimArena, game_state* GameState,
    world* World, world_position Origin, rectangle3 Bounds
) {
    sim_region* SimRegion = PushStruct(SimArena, sim_region);
    ZeroStruct(SimRegion->Hash);

    SimRegion->World = World;

    SimRegion->Origin = Origin;
    SimRegion->UpdatableBounds = Bounds;
    real32 UpdateSafetyMargin = 1.0f;
    real32 UpdateSafetyMarginZ = 1.0f;
    SimRegion->Bounds = AddRadius(
        SimRegion->UpdatableBounds,
        V3(UpdateSafetyMargin, UpdateSafetyMargin, UpdateSafetyMarginZ)
    );

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

                    if (!IsSet(&Low->Sim, EntityFlag_Nonspatial)) {
                        v3 SimSpaceP = GetSimSpaceP(SimRegion, Low);
                        if (IsInRectangle(Bounds, SimSpaceP)) {
                            AddEntity(GameState, SimRegion, LowEntityIndex, Low, &SimSpaceP);
                        }
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
    world_position NewPInit
) {
    world_position* OldP = 0;
    world_position* NewP = 0;

    if (!IsSet(&EntityLow->Sim, EntityFlag_Nonspatial) && IsValid(EntityLow->P)) {
        OldP = &EntityLow->P;
    }

    if (IsValid(NewPInit)) {
        NewP = &NewPInit;
    }

    ChangeEntityLocationRaw(Arena, World, LowEntityIndex, OldP, NewP);
    if (NewP) {
        EntityLow->P = *NewP;
        ClearFlag(&EntityLow->Sim, EntityFlag_Nonspatial);
    } else {
        EntityLow->P = NullPosition();
        AddFlag(&EntityLow->Sim, EntityFlag_Nonspatial);
    }
}

internal void EndSim(sim_region* Region, game_state* GameState) {

    sim_entity* Entity = Region->Entities;

    for (uint32 EntityIndex = 0;
        EntityIndex < Region->EntityCount;
        EntityIndex++, Entity++) {

        //! stored_entity
        low_entity_* Stored = GameState->LowEntities + Entity->StorageIndex;

        Assert(IsSet(&Stored->Sim, EntityFlag_Simming));

        Stored->Sim = *Entity;
        Assert(!IsSet(&Stored->Sim, EntityFlag_Simming));

        StoreEntityReference(&Stored->Sim.Sword);

        world_position NewP =
            IsSet(Entity, EntityFlag_Nonspatial) ?
            NullPosition() :
            MapIntoChunkSpace(GameState->World, Region->Origin, Entity->P);

        ChangeEntityLocation(
            &GameState->WorldArena, GameState->World, Entity->StorageIndex, Stored, NewP
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
            real32 CamZOffset = NewCameraP.Offset_.Z;
            NewCameraP = Stored->P;
            NewCameraP.Offset_.Z = CamZOffset;
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

internal void
AddCollisionRule(
    game_state* GameState, uint32 StorageIndexA, uint32 StorageIndexB, bool32 ShouldCollide
) {

    if (StorageIndexA > StorageIndexB) {
        uint32 Temp = StorageIndexA;
        StorageIndexA = StorageIndexB;
        StorageIndexB = Temp;
    }

    pairwise_collision_rule* Found = 0;

    uint32 HashBucket = StorageIndexA & (ArrayCount(GameState->CollisionRuleHash) - 1);
    for (pairwise_collision_rule* Rule = GameState->CollisionRuleHash[HashBucket];
        Rule;
        Rule = Rule->NextInHash) {

        if (Rule->StorageIndexA == StorageIndexB && Rule->StorageIndexB == StorageIndexB) {
            Found = Rule;
            break;
        }
    }

    if (!Found) {
        Found = GameState->FirstFreeCollisionRule;
        if (Found) {
            GameState->FirstFreeCollisionRule = Found->NextInHash;
        } else {
            Found = PushStruct(&GameState->WorldArena, pairwise_collision_rule);
        }
        Found->NextInHash = GameState->CollisionRuleHash[HashBucket];
        GameState->CollisionRuleHash[HashBucket] = Found;
    }

    if (Found) {
        Found->StorageIndexA = StorageIndexA;
        Found->StorageIndexB = StorageIndexB;
        Found->ShouldCollide = ShouldCollide;
    }
}

internal void ClearCollisionRules(game_state* GameState, uint32 StorageIndex) {
    for (uint32 HashIndex = 0; HashIndex < ArrayCount(GameState->CollisionRuleHash); HashIndex++) {
        for (pairwise_collision_rule** Rule = GameState->CollisionRuleHash + HashIndex;
            *Rule;
            ) {
            if ((*Rule)->StorageIndexA == StorageIndex || (*Rule)->StorageIndexB == StorageIndex) {
                pairwise_collision_rule* RemovedRule = *Rule;
                *Rule = (*Rule)->NextInHash;

                RemovedRule->NextInHash = GameState->FirstFreeCollisionRule;
                GameState->FirstFreeCollisionRule = RemovedRule;
            } else {
                Rule = &(*Rule)->NextInHash;
            }
        }
    }
}

internal bool32 ShouldCollide(game_state* GameState, sim_entity* A, sim_entity* B) {

    bool32 Result = false;

    if (A == B) {
        return Result;
    }

    if (A->StorageIndex > B->StorageIndex) {
        sim_entity* Temp = A;
        A = B;
        B = Temp;
    }

    if (!IsSet(A, EntityFlag_Nonspatial) &&
        !IsSet(B, EntityFlag_Nonspatial)) {
        Result = true;
    }

    uint32 HashBucket = A->StorageIndex & (ArrayCount(GameState->CollisionRuleHash) - 1);
    for (pairwise_collision_rule* Rule = GameState->CollisionRuleHash[HashBucket];
        Rule;
        Rule = Rule->NextInHash) {

        if (Rule->StorageIndexA == A->StorageIndex && Rule->StorageIndexB == B->StorageIndex) {
            Result = Rule->ShouldCollide;
            break;
        }
    }

    return Result;
}

internal bool32 HandleCollision(sim_entity* A, sim_entity* B) {

    bool32 StopsOnCollisiton = false;

    if (A->Type != EntityType_Sword) {
        StopsOnCollisiton = true;
    }

    if (A->Type > B->Type) {
        sim_entity* Temp = A;
        A = B;
        B = Temp;
    }

    if (A->Type == EntityType_Monster && B->Type == EntityType_Sword) {
        if (A->HitPointMax > 0) {
            --A->HitPointMax;
        }
    }

    return StopsOnCollisiton;
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
MoveEntity(
    game_state* GameState,
    sim_region* Region, sim_entity* Entity, real32 dt, move_spec* MoveSpec, v3 ddPlayer
) {

    Assert(!IsSet(Entity, EntityFlag_Nonspatial));

    world* World = Region->World;

    if (MoveSpec->UnitMaxAccelVector) {
        real32 ddPlayerLengthSq = LengthSq(ddPlayer);
        if (ddPlayerLengthSq > 1.0f) {
            ddPlayer *= 1.0f / SquareRoot(ddPlayerLengthSq);
        }
    }

    ddPlayer *= MoveSpec->Speed;

    ddPlayer += -MoveSpec->Drag * Entity->dP;

    ddPlayer += V3(0.0f, 0.0f, -9.8f);

    v3 OldPlayerP = Entity->P;

    v3 PlayerDelta = 0.5f * ddPlayer * Square(dt) + Entity->dP * dt;

    v3 NewPlayerP = OldPlayerP + PlayerDelta;
    Entity->dP += ddPlayer * dt;

    real32 DistanceRemaining = Entity->DistanceLimit;
    if (DistanceRemaining <= 0.0f) {
        DistanceRemaining = 1e5;
    }

    for (uint32 Iteration = 0; Iteration < 4; ++Iteration) {

        real32 PlayerDeltaLength = Length(PlayerDelta);

        if (PlayerDeltaLength <= 0.0f) {
            break;
        }

        real32 tMin = 1.0f;

        if (PlayerDeltaLength > DistanceRemaining) {
            tMin = DistanceRemaining / PlayerDeltaLength;
        }

        v3 WallNormal = {};
        sim_entity* HitEntity = 0;

        v3 DesiredPosition = Entity->P + PlayerDelta;

        if (!IsSet(Entity, EntityFlag_Nonspatial)) {

            for (uint32 TestHighEntityIndex = 1;
                TestHighEntityIndex < Region->EntityCount;
                ++TestHighEntityIndex) {

                sim_entity* TestEntity = Region->Entities + TestHighEntityIndex;

                if (!ShouldCollide(GameState, Entity, TestEntity)) {
                    continue;
                }

                v3 MinkowskiDiameter = V3(
                    Entity->Width + TestEntity->Width,
                    Entity->Height + TestEntity->Height,
                    2.0f * World->TileDepthInMeters
                );

                v3 MinCorner = -0.5f * MinkowskiDiameter;
                v3 MaxCorner = 0.5f * MinkowskiDiameter;

                v3 Rel = Entity->P - TestEntity->P;

                if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y)) {
                    WallNormal = V3(-1, 0, 0);
                    HitEntity = TestEntity;
                }
                if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y)) {
                    WallNormal = V3(1, 0, 0);
                    HitEntity = TestEntity;
                }
                if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X)) {
                    WallNormal = V3(0, -1, 0);
                    HitEntity = TestEntity;
                }
                if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X)) {
                    WallNormal = V3(0, 1, 0);
                    HitEntity = TestEntity;
                }
            }
        }

        Entity->P += tMin * PlayerDelta;
        DistanceRemaining -= tMin * PlayerDeltaLength;

        if (HitEntity != 0) {

            PlayerDelta = DesiredPosition - Entity->P;

            bool32 StopsOnCollision = HandleCollision(Entity, HitEntity);

            if (StopsOnCollision) {
                PlayerDelta = PlayerDelta - Inner(PlayerDelta, WallNormal) * WallNormal;
                Entity->dP = Entity->dP - Inner(Entity->dP, WallNormal) * WallNormal;
            } else {
                AddCollisionRule(GameState, Entity->StorageIndex, HitEntity->StorageIndex, false);
            }

        } else {
            break;
        }

    }

    if (Entity->P.Z < 0) {
        Entity->P.Z = 0;
    }

    if (Entity->DistanceLimit != 0.0f) {
        Entity->DistanceLimit = DistanceRemaining;
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
