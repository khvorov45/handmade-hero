#if !defined(HANDMADE_SIM_REGION_CPP)
#define HANDMADE_SIM_REGION_CPP

#include "../types.h"
#include "world.cpp"
#include "math.cpp"
#include "bmp.cpp"

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

struct controlled_hero {
    uint32 EntityIndex;
    v2 ddP;
    v2 dSword;
    real32 dZ;
};


struct sim_entity;
union entity_reference {
    sim_entity* Ptr;
    uint32 Index;
};

enum sim_entity_flags {
    EntityFlag_Collides = 1 << 0,
    EntityFlag_Nonspatial = 1 << 1,
    EntityFlag_Moveable = 1 << 2,
    EntityFlag_ZSupported = 1 << 3,
    EntityFlag_Traversable = 1 << 4,

    EntityFlag_Simming = 1 << 30
};

struct sim_entity_collision_volume {
    v3 OffsetP;
    v3 Dim;
};

struct sim_entity_collision_volume_group {
    sim_entity_collision_volume TotalVolume;

    uint32 VolumeCount;
    sim_entity_collision_volume* Volumes;
};

struct sim_entity {
    uint32 StorageIndex;
    bool32 Updatable;

    entity_type Type;
    uint32 Flags;

    v3 P;
    v3 dP;

    real32 DistanceLimit;

    sim_entity_collision_volume_group* Collision;

    uint32 FacingDirection;
    real32 tBob;

    int32 dAbsTileZ; //* Stairs

    uint32 HitPointMax;
    hit_point HitPoint[16];

    entity_reference Sword;

    v2 WalkableDim;
    real32 WalkableHeight;
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
    real32 MaxEntityRadius;
    real32 MaxEntityVelocity;

    world_position Origin;
    rectangle3 Bounds;
    rectangle3 UpdatableBounds;

    uint32 MaxEntityCount;
    uint32 EntityCount;
    sim_entity* Entities;

    sim_entity_hash Hash[4096];
};

struct pairwise_collision_rule {
    bool32 CanCollide;
    uint32 StorageIndexA;
    uint32 StorageIndexB;

    pairwise_collision_rule* NextInHash;
};

struct ground_buffer {
    world_position P;
    loaded_bitmap Bitmap;
};

struct game_state {
    memory_arena WorldArena;

    world* World;
    uint32 CameraFollowingEntityIndex;
    world_position CameraP;

    real32 TypicalFloorHeight;

    controlled_hero ControlledHeroes[ArrayCount(((game_input*)0)->Controllers)];

    uint32 LowEntity_Count;
    low_entity_ LowEntities[1000000];

    loaded_bitmap Grass[2];
    loaded_bitmap Ground[4];
    loaded_bitmap Tuft[3];

    loaded_bitmap Backdrop;
    hero_bitmaps HeroBitmaps[4];
    loaded_bitmap HeroShadow;

    loaded_bitmap Tree;
    loaded_bitmap Sword;
    loaded_bitmap Stairwell;

    real32 MetersToPixels;
    real32 PixelsToMeters;

    pairwise_collision_rule* CollisionRuleHash[256];
    pairwise_collision_rule* FirstFreeCollisionRule;

    sim_entity_collision_volume_group* NullCollision;
    sim_entity_collision_volume_group* StandardRoomCollision;
    sim_entity_collision_volume_group* SwordCollision;
    sim_entity_collision_volume_group* StairCollision;
    sim_entity_collision_volume_group* PlayerCollision;
    sim_entity_collision_volume_group* MonsterCollision;
    sim_entity_collision_volume_group* WallCollision;
    sim_entity_collision_volume_group* FamiliarCollision;
};

struct transient_state {
    bool32 IsInitialized;
    memory_arena TranArena;
    uint32 GroundBufferCount;
    ground_buffer* GroundBuffers;
};

internal bool32 IsSet(sim_entity* Entity, uint32 Flag) {
    bool32 Result = (Entity->Flags & Flag) != 0;
    return Result;
}

internal void AddFlags(sim_entity* Entity, uint32 Flag) {
    Entity->Flags |= Flag;
}

internal void ClearFlags(sim_entity* Entity, uint32 Flag) {
    Entity->Flags &= ~Flag;
}

#define InvalidP { 100000.0f, 100000.0f, 100000.0f }

internal void MakeEntityNonSpatial(sim_entity* Entity) {
    AddFlags(Entity, EntityFlag_Nonspatial);
    Entity->P = InvalidP;
}

internal void MakeEntitySpatial(sim_entity* Entity, v3 P, v3 dP) {
    ClearFlags(Entity, EntityFlag_Nonspatial);
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
                AddFlags(&Source->Sim, EntityFlag_Simming);
            }
            Entity->StorageIndex = StorageIndex;
            Entity->Updatable = false;

        } else {
            InvalidCodePath;
        }
    }

    return Entity;
}

internal bool32 EntityOverlapsRectange(v3 P, sim_entity_collision_volume Volume, rectangle3 Rect) {

    rectangle3 GrownRect = AddRadius(Rect, 0.5f * Volume.Dim);

    bool32 Result = IsInRectangle(GrownRect, P + Volume.OffsetP);

    return Result;
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
            Dest->Updatable =
                EntityOverlapsRectange(Dest->P, Dest->Collision->TotalVolume, SimRegion->UpdatableBounds);
        } else {
            Dest->P = GetSimSpaceP(SimRegion, Source);
        }
    }
    return Dest;
}

internal sim_region*
BeginSim(
    memory_arena* SimArena, game_state* GameState,
    world* World, world_position Origin, rectangle3 Bounds, real32 dt
) {
    sim_region* SimRegion = PushStruct(SimArena, sim_region);
    ZeroStruct(SimRegion->Hash);

    SimRegion->World = World;

    SimRegion->Origin = Origin;

    SimRegion->MaxEntityRadius = 5.0f;
    SimRegion->MaxEntityVelocity = 30.0f;
    real32 UpdateSafetyMargin = SimRegion->MaxEntityRadius + SimRegion->MaxEntityVelocity * dt;
    real32 UpdateSafetyMarginZ = 1.0f;

    SimRegion->UpdatableBounds = AddRadius(
        Bounds,
        V3(SimRegion->MaxEntityRadius, SimRegion->MaxEntityRadius, SimRegion->MaxEntityRadius)
    );

    SimRegion->Bounds = AddRadius(
        SimRegion->UpdatableBounds,
        V3(UpdateSafetyMargin, UpdateSafetyMargin, UpdateSafetyMarginZ)
    );

    SimRegion->MaxEntityCount = 4096;
    SimRegion->EntityCount = 0;
    SimRegion->Entities = PushArray(SimArena, SimRegion->MaxEntityCount, sim_entity);

    world_position MinChunkP =
        MapIntoChunkSpace(World, Origin, GetMinCorner(SimRegion->Bounds));
    world_position MaxChunkP =
        MapIntoChunkSpace(World, Origin, GetMaxCorner(SimRegion->Bounds));

    for (int32 ChunkZ = MinChunkP.ChunkZ; ChunkZ <= MaxChunkP.ChunkZ; ChunkZ++) {

        for (int32 ChunkY = MinChunkP.ChunkY; ChunkY <= MaxChunkP.ChunkY; ChunkY++) {

            for (int32 ChunkX = MinChunkP.ChunkX; ChunkX <= MaxChunkP.ChunkX; ChunkX++) {

                world_chunk* Chunk = GetChunk(World, ChunkX, ChunkY, ChunkZ);
                if (Chunk == 0) {
                    continue;
                }

                for (world_entity_block* Block = &Chunk->FirstBlock; Block; Block = Block->Next) {

                    for (uint32 BlockIndex = 0; BlockIndex < Block->EntityCount; ++BlockIndex) {

                        uint32 LowEntityIndex = Block->LowEntityIndex[BlockIndex];
                        low_entity_* Low = GameState->LowEntities + LowEntityIndex;

                        if (!IsSet(&Low->Sim, EntityFlag_Nonspatial)) {
                            v3 SimSpaceP = GetSimSpaceP(SimRegion, Low);
                            if (EntityOverlapsRectange(SimSpaceP, Low->Sim.Collision->TotalVolume, SimRegion->Bounds)) {
                                AddEntity(GameState, SimRegion, LowEntityIndex, Low, &SimSpaceP);
                            }
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
        ClearFlags(&EntityLow->Sim, EntityFlag_Nonspatial);
    } else {
        EntityLow->P = NullPosition();
        AddFlags(&EntityLow->Sim, EntityFlag_Nonspatial);
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

struct test_wall {
    real32 X, RelX, RelY, DeltaX, DeltaY, MinY, MaxY;
    v3 Normal;
};

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
    game_state* GameState, uint32 StorageIndexA, uint32 StorageIndexB, bool32 CanCollide
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
        Found->CanCollide = CanCollide;
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

internal bool32 CanOverlap(game_state* GameState, sim_entity* Mover, sim_entity* Region) {

    bool32 Result = false;

    if (Mover == Region) {
        return Result;
    }

    if (Region->Type == EntityType_Stairwell) {
        Result = true;
    }

    return Result;
}

inline real32 GetStairGround(sim_entity* Entity, v3 AtGroudPoint) {

    Assert(Entity->Type == EntityType_Stairwell);

    rectangle2 RegionRect = RectCenterDim(Entity->P.XY, Entity->WalkableDim);
    v2 Bary = Clamp01(GetBarycentric(RegionRect, AtGroudPoint.XY));
    real32 Result = Entity->P.Z + Bary.Y * Entity->WalkableHeight;

    return Result;
}

inline v3 GetEntityGroundPoint(sim_entity* Entity, v3 ForEntityP) {
    v3 Result = ForEntityP;
    return Result;
}

inline v3 GetEntityGroundPoint(sim_entity* Entity) {
    v3 Result = GetEntityGroundPoint(Entity, Entity->P);
    return Result;
}

internal void
HandleOverlap(
    game_state* GameState, sim_entity* Mover, sim_entity* Region, real32 dt, real32* Ground
) {
    if (Region->Type == EntityType_Stairwell) {
        *Ground = GetStairGround(Region, GetEntityGroundPoint(Mover));
    }
}

internal bool32 CanCollide(game_state* GameState, sim_entity* A, sim_entity* B) {

    bool32 Result = false;

    if (A == B) {
        return Result;
    }

    if (A->StorageIndex > B->StorageIndex) {
        sim_entity* Temp = A;
        A = B;
        B = Temp;
    }

    if (IsSet(A, EntityFlag_Collides) && IsSet(B, EntityFlag_Collides)) {

        if (!IsSet(A, EntityFlag_Nonspatial) &&
            !IsSet(B, EntityFlag_Nonspatial)) {
            Result = true;
        }

        uint32 HashBucket = A->StorageIndex & (ArrayCount(GameState->CollisionRuleHash) - 1);
        for (pairwise_collision_rule* Rule = GameState->CollisionRuleHash[HashBucket];
            Rule;
            Rule = Rule->NextInHash) {

            if (Rule->StorageIndexA == A->StorageIndex && Rule->StorageIndexB == B->StorageIndex) {
                Result = Rule->CanCollide;
                break;
            }
        }
    }

    return Result;
}

internal bool32
HandleCollision(game_state* GameState, sim_entity* A, sim_entity* B) {

    bool32 StopsOnCollisiton = false;

    if (A->Type == EntityType_Sword) {
        AddCollisionRule(GameState, A->StorageIndex, B->StorageIndex, false);
        StopsOnCollisiton = false;
    } else {
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

internal bool32 SpeculativeCollide(sim_entity* Mover, sim_entity* Region, v3 TestP) {
    bool32 Result = true;
    if (Region->Type == EntityType_Stairwell) {
        v3 MoverGroundPoint = GetEntityGroundPoint(Mover, TestP);
        real32 Ground = GetStairGround(Region, MoverGroundPoint);
        real32 StepHeight = 0.1f;
        Result = AbsoluteValue(MoverGroundPoint.Z - Ground) > StepHeight;
    }
    return Result;
}

internal bool32
EntitiesOverlap(sim_entity* Entity, sim_entity* TestEntity, v3 Epsilon = V3(0, 0, 0)) {

    bool32 Result = false;

    for (uint32 VolumeIndex = 0;
        !Result && VolumeIndex < Entity->Collision->VolumeCount;
        VolumeIndex++) {

        for (uint32 TestVolumeIndex = 0;
            !Result && TestVolumeIndex < TestEntity->Collision->VolumeCount;
            TestVolumeIndex++) {

            sim_entity_collision_volume* Volume =
                Entity->Collision->Volumes + VolumeIndex;

            sim_entity_collision_volume* TestVolume =
                TestEntity->Collision->Volumes + TestVolumeIndex;

            rectangle3 EntityRect =
                RectCenterDim(Entity->P + Volume->OffsetP, Volume->Dim + Epsilon);
            rectangle3 TestEntityRect =
                RectCenterDim(TestEntity->P + TestVolume->OffsetP, TestVolume->Dim);

            Result = RectanglesIntersect(EntityRect, TestEntityRect);
        }
    }

    return Result;
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
    v3 Drag = -MoveSpec->Drag * Entity->dP;
    Drag.Z = 0;
    ddPlayer += Drag;

    if (!IsSet(Entity, EntityFlag_ZSupported)) {
        ddPlayer += V3(0.0f, 0.0f, -9.8f);
    }

    v3 OldPlayerP = Entity->P;

    v3 PlayerDelta = 0.5f * ddPlayer * Square(dt) + Entity->dP * dt;

    v3 NewPlayerP = OldPlayerP + PlayerDelta;
    Entity->dP += ddPlayer * dt;
    Assert(LengthSq(Entity->dP) <= Square(Region->MaxEntityVelocity));

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
        real32 tMax = 0.0f;

        if (PlayerDeltaLength > DistanceRemaining) {
            tMin = DistanceRemaining / PlayerDeltaLength;
        }

        v3 WallNormalMin = {};
        v3 WallNormalMax = {};
        sim_entity* HitEntityMin = 0;
        sim_entity* HitEntityMax = 0;

        v3 DesiredPosition = Entity->P + PlayerDelta;

        if (!IsSet(Entity, EntityFlag_Nonspatial)) {

            for (uint32 TestHighEntityIndex = 1;
                TestHighEntityIndex < Region->EntityCount;
                ++TestHighEntityIndex) {

                sim_entity* TestEntity = Region->Entities + TestHighEntityIndex;

                real32 OverlapEpsilon = 0.001f;

                if ((IsSet(TestEntity, EntityFlag_Traversable) &&
                    EntitiesOverlap(Entity, TestEntity, OverlapEpsilon * V3(1.0f, 1.0f, 1.0f))) ||
                    CanCollide(GameState, Entity, TestEntity)) {

                    for (uint32 VolumeIndex = 0;
                        VolumeIndex < Entity->Collision->VolumeCount;
                        VolumeIndex++) {

                        for (uint32 TestVolumeIndex = 0;
                            TestVolumeIndex < TestEntity->Collision->VolumeCount;
                            TestVolumeIndex++) {

                            sim_entity_collision_volume* Volume =
                                Entity->Collision->Volumes + VolumeIndex;

                            sim_entity_collision_volume* TestVolume =
                                TestEntity->Collision->Volumes + TestVolumeIndex;

                            v3 MinkowskiDiameter = Volume->Dim + TestVolume->Dim;

                            v3 MinCorner = -0.5f * MinkowskiDiameter;
                            v3 MaxCorner = 0.5f * MinkowskiDiameter;

                            v3 Rel =
                                (Entity->P + Volume->OffsetP) - (TestEntity->P + TestVolume->OffsetP);

                            if (Rel.Z < MinCorner.Z || Rel.Z >= MaxCorner.Z) {
                                continue;
                            }

                            test_wall Walls[] = {
                                {MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, MinCorner.Y, MaxCorner.Y, V3(-1, 0, 0)},
                                {MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, MinCorner.Y, MaxCorner.Y, V3(1, 0, 0)},
                                {MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, MinCorner.X, MaxCorner.X, V3(0, -1, 0)},
                                {MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, MinCorner.X, MaxCorner.X, V3(0, 1, 0)}
                            };

                            if (IsSet(TestEntity, EntityFlag_Traversable)) {
                                real32 tMaxTest = tMax;
                                bool32 HitThis = false;
                                v3 TestWallNormal = {};
                                for (uint32 WallIndex = 0; WallIndex < ArrayCount(Walls); ++WallIndex) {

                                    test_wall* Wall = Walls + WallIndex;

                                    if (Wall->DeltaX != 0.0f) {
                                        real32 tResult = (Wall->X - Wall->RelX) / Wall->DeltaX;
                                        real32 Y = Wall->RelY + tResult * Wall->DeltaY;
                                        if (tResult >= 0.0f && tMaxTest < tResult) {
                                            if (Y >= Wall->MinY && Y <= Wall->MaxY) {
                                                real32 tEpsilon = 0.001f;
                                                tMaxTest = Maximum(0.0f, tResult - tEpsilon);
                                                TestWallNormal = Wall->Normal;
                                                HitThis = true;
                                            }
                                        }
                                    }
                                }
                                if (HitThis) {
                                    tMax = tMaxTest;
                                    WallNormalMax = TestWallNormal;
                                    HitEntityMax = TestEntity;
                                }

                            } else {
                                real32 tMinTest = tMin;
                                bool32 HitThis = false;
                                v3 TestWallNormal = {};
                                for (uint32 WallIndex = 0; WallIndex < ArrayCount(Walls); ++WallIndex) {

                                    test_wall* Wall = Walls + WallIndex;

                                    if (Wall->DeltaX != 0.0f) {
                                        real32 tResult = (Wall->X - Wall->RelX) / Wall->DeltaX;
                                        real32 Y = Wall->RelY + tResult * Wall->DeltaY;
                                        if (tResult >= 0.0f && tMinTest > tResult) {
                                            if (Y >= Wall->MinY && Y <= Wall->MaxY) {
                                                real32 tEpsilon = 0.001f;
                                                tMinTest = Maximum(0.0f, tResult - tEpsilon);
                                                TestWallNormal = Wall->Normal;
                                                HitThis = true;
                                            }
                                        }
                                    }
                                }
                                if (HitThis) {
                                    v3 TestP = Entity->P + tMinTest * PlayerDelta;
                                    if (SpeculativeCollide(Entity, TestEntity, TestP)) {
                                        tMin = tMinTest;
                                        WallNormalMin = TestWallNormal;
                                        HitEntityMin = TestEntity;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        real32 tStop = 0.0f;
        sim_entity* HitEntity = 0;
        v3 WallNormal = {};
        if (tMin < tMax) {
            tStop = tMin;
            HitEntity = HitEntityMin;
            WallNormal = WallNormalMin;
        } else {
            tStop = tMax;
            HitEntity = HitEntityMax;
            WallNormal = WallNormalMax;
        }

        Entity->P += tStop * PlayerDelta;
        DistanceRemaining -= tStop * PlayerDeltaLength;

        if (HitEntity != 0) {

            PlayerDelta = DesiredPosition - Entity->P;

            bool32 StopsOnCollision = HandleCollision(GameState, Entity, HitEntity);

            if (StopsOnCollision) {
                PlayerDelta = PlayerDelta - Inner(PlayerDelta, WallNormal) * WallNormal;
                Entity->dP = Entity->dP - Inner(Entity->dP, WallNormal) * WallNormal;
            }

        } else {
            break;
        }
    }

    real32 Ground = 0.0f;
    {
        for (uint32 TestHighEntityIndex = 1;
            TestHighEntityIndex < Region->EntityCount;
            ++TestHighEntityIndex) {

            sim_entity* TestEntity = Region->Entities + TestHighEntityIndex;

            if (CanOverlap(GameState, Entity, TestEntity) && EntitiesOverlap(Entity, TestEntity)) {
                HandleOverlap(GameState, Entity, TestEntity, dt, &Ground);
            }
        }
    }

    Ground += Entity->P.Z - GetEntityGroundPoint(Entity).Z;
    if (Entity->P.Z <= Ground || (IsSet(Entity, EntityFlag_ZSupported) && Entity->dP.Z == 0)) {
        Entity->P.Z = Ground;
        Entity->dP.Z = 0;
        AddFlags(Entity, EntityFlag_ZSupported);
    } else {
        ClearFlags(Entity, EntityFlag_ZSupported);
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
