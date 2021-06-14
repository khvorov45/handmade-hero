#include "sim_region.cpp"

struct add_low_entity_result_ {
    low_entity_* Low;
    uint32 LowIndex;
};

internal add_low_entity_result_
AddLowEntity_(game_state* GameState, entity_type EntityType, world_position* Position) {

    Assert(GameState->LowEntity_Count < ArrayCount(GameState->LowEntities));

    add_low_entity_result_ Result = {};

    Result.LowIndex = GameState->LowEntity_Count++;

    Result.Low = GameState->LowEntities + Result.LowIndex;
    *Result.Low = {};
    Result.Low->Sim.Type = EntityType;

    ChangeEntityLocation(
        &GameState->WorldArena, GameState->World, Result.LowIndex, Result.Low, 0, Position
    );


    return Result;
}

internal void InitHitpoints_(low_entity_* EntityLow, uint32 Max) {
    Assert(Max <= ArrayCount(EntityLow->Sim.HitPoint));
    EntityLow->Sim.HitPointMax = Max;
    for (uint32 HealthIndex = 0; HealthIndex < Max; HealthIndex++) {
        EntityLow->Sim.HitPoint[HealthIndex].FilledAmount = HITPOINT_SUBCOUNT;
    }
}

internal add_low_entity_result_ AddSword_(game_state* GameState) {

    add_low_entity_result_ Entity = AddLowEntity_(GameState, EntityType_Sword, 0);

    Entity.Low->Sim.Height = 0.5f;
    Entity.Low->Sim.Width = 1.0f;

    return Entity;
}

internal add_low_entity_result_ AddPlayer_(game_state* GameState) {

    add_low_entity_result_ Entity = AddLowEntity_(GameState, EntityType_Hero, &GameState->CameraP);

    Entity.Low->Sim.Height = 0.5f;
    Entity.Low->Sim.Width = 1.0f;

    AddFlag(&Entity.Low->Sim, EntityFlag_Collides);

    InitHitpoints_(Entity.Low, 3);

    add_low_entity_result_ Sword = AddSword_(GameState);
    Entity.Low->Sim.Sword.Index = Sword.LowIndex;

    if (GameState->CameraFollowingEntityIndex == 0) {
        GameState->CameraFollowingEntityIndex = Entity.LowIndex;
    }

    return Entity;
}

internal add_low_entity_result_
AddWall_(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity = AddLowEntity_(GameState, EntityType_Wall, &EntityLowP);

    real32 TileSide = GameState->World->TileSideInMeters;

    Entity.Low->Sim.Height = TileSide;
    Entity.Low->Sim.Width = TileSide;

    AddFlag(&Entity.Low->Sim, EntityFlag_Collides);

    return Entity;
}

internal add_low_entity_result_
AddMonster_(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity = AddLowEntity_(GameState, EntityType_Monster, &EntityLowP);

    Entity.Low->Sim.Height = 0.5f;
    Entity.Low->Sim.Width = 1.0f;

    InitHitpoints_(Entity.Low, 3);

    AddFlag(&Entity.Low->Sim, EntityFlag_Collides);

    return Entity;
}

internal add_low_entity_result_
AddFamiliar_(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity = AddLowEntity_(GameState, EntityType_Familiar, &EntityLowP);

    Entity.Low->Sim.Height = 0.5f;
    Entity.Low->Sim.Width = 1.0f;

    AddFlag(&Entity.Low->Sim, EntityFlag_Collides);

    return Entity;
}

// ================================================================================================

internal void UpdateFamiliar_(sim_region* SimRegion, sim_entity* Entity, real32 dt) {

    sim_entity* ClosestHero = 0;
    real32 ClosestHeroDSq = Square(15.0f);

    for (uint32 TestEntityIndex = 0;
        TestEntityIndex < SimRegion->EntityCount;
        TestEntityIndex++) {

        sim_entity* TestEntity = SimRegion->Entities + TestEntityIndex;

        if (TestEntity->Type == EntityType_Hero) {
            real32 TestDSq = LengthSq(TestEntity->P - Entity->P);
            if (TestDSq < ClosestHeroDSq) {
                ClosestHeroDSq = TestDSq;
                ClosestHero = TestEntity;
            }
        }
    }
    v2 ddP = {};
    if (ClosestHero && ClosestHeroDSq > Square(3.0f)) {
        real32 Acceleration = 0.3f;
        real32 OneOverLength = (Acceleration / SquareRoot(ClosestHeroDSq));
        ddP = (ClosestHero->P - Entity->P) * OneOverLength;
    }
    move_spec MoveSpec = DefaultMoveSpec();
    MoveSpec.Drag = 8.0f;
    MoveSpec.Speed = 150.0f;
    MoveSpec.UnitMaxAccelVector = true;
    MoveEntity(SimRegion, Entity, dt, &MoveSpec, ddP);
}

internal void UpdateMonster_(sim_region* SimRegion, sim_entity* Entity, real32 dt) {}

internal void
UpdateSword_(sim_region* SimRegion, sim_entity* Entity, real32 dt) {
    move_spec MoveSpec = DefaultMoveSpec();
    MoveSpec.Speed = 0.0f;
    MoveSpec.Drag = 0.0f;
    MoveSpec.UnitMaxAccelVector = false;

    v2 OldP = Entity->P;
    MoveEntity(SimRegion, Entity, dt, &MoveSpec, { 0.0f, 0.0f });
    real32 DistanceTravelled = Length(OldP - Entity->P);

    Entity->DistanceRemaining -= DistanceTravelled;
    if (Entity->DistanceRemaining < 0.0f) {
        Assert(!"MAKE ENTITY NOT BE THERE")
    }
}
