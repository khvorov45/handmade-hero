#include "sim_region.cpp"
#include "math.cpp"

struct add_low_entity_result_ {
    low_entity_* Low;
    uint32 LowIndex;
};

internal add_low_entity_result_
AddLowEntity_(game_state* GameState, entity_type EntityType, world_position Position) {

    Assert(GameState->LowEntity_Count < ArrayCount(GameState->LowEntities));

    add_low_entity_result_ Result = {};

    Result.LowIndex = GameState->LowEntity_Count++;

    Result.Low = GameState->LowEntities + Result.LowIndex;
    *Result.Low = {};
    Result.Low->Sim.Type = EntityType;
    Result.Low->P = NullPosition();

    ChangeEntityLocation(
        &GameState->WorldArena, GameState->World, Result.LowIndex, Result.Low, Position
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

    add_low_entity_result_ Entity = AddLowEntity_(GameState, EntityType_Sword, NullPosition());

    Entity.Low->Sim.Dim.Y = 0.5f;
    Entity.Low->Sim.Dim.X = 1.0f;
    AddFlags(&Entity.Low->Sim, EntityFlag_Moveable);

    return Entity;
}

internal add_low_entity_result_ AddPlayer_(game_state* GameState) {

    add_low_entity_result_ Entity = AddLowEntity_(GameState, EntityType_Hero, GameState->CameraP);

    Entity.Low->Sim.Dim.Y = 0.5f;
    Entity.Low->Sim.Dim.X = 1.0f;

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides | EntityFlag_Moveable);

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

    add_low_entity_result_ Entity = AddLowEntity_(GameState, EntityType_Wall, EntityLowP);

    real32 TileSide = GameState->World->TileSideInMeters;

    Entity.Low->Sim.Dim.Y = TileSide;
    Entity.Low->Sim.Dim.X = TileSide;

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides);

    return Entity;
}

internal add_low_entity_result_
AddMonster_(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity = AddLowEntity_(GameState, EntityType_Monster, EntityLowP);

    Entity.Low->Sim.Dim.Y = 0.5f;
    Entity.Low->Sim.Dim.X = 1.0f;

    InitHitpoints_(Entity.Low, 3);

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides | EntityFlag_Moveable);

    return Entity;
}

internal add_low_entity_result_
AddFamiliar_(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity = AddLowEntity_(GameState, EntityType_Familiar, EntityLowP);

    Entity.Low->Sim.Dim.Y = 0.5f;
    Entity.Low->Sim.Dim.X = 1.0f;

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides | EntityFlag_Moveable);

    return Entity;
}

internal add_low_entity_result_
AddStair(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position EntityLowP = ChunkPositionFromTilePosition(
        GameState->World, AbsTileX, AbsTileY, AbsTileZ,
        V3(0, 0, 0.5f * GameState->World->TileDepthInMeters)
    );

    add_low_entity_result_ Entity = AddLowEntity_(GameState, EntityType_Stairwell, EntityLowP);

    Entity.Low->Sim.Dim.X = GameState->World->TileSideInMeters;
    Entity.Low->Sim.Dim.Y = 2.0f * GameState->World->TileSideInMeters;
    Entity.Low->Sim.Dim.Z = GameState->World->TileDepthInMeters;

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides);

    return Entity;
}

// ================================================================================================
