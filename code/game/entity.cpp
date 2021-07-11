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
    Result.Low->Sim.Collision = GameState->NullCollision;

    ChangeEntityLocation(
        &GameState->WorldArena, GameState->World, Result.LowIndex, Result.Low, Position
    );

    return Result;
}

internal add_low_entity_result_ AddGroundedEntity(
    game_state* GameState,
    entity_type EntityType,
    world_position Position,
    sim_entity_collision_volume_group* Collision
) {
    add_low_entity_result_ Result = AddLowEntity_(GameState, EntityType, Position);
    Result.Low->Sim.Collision = Collision;
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

    Entity.Low->Sim.Collision = GameState->SwordCollision;

    AddFlags(&Entity.Low->Sim, EntityFlag_Moveable);

    return Entity;
}

internal add_low_entity_result_ AddPlayer_(game_state* GameState) {

    add_low_entity_result_ Entity =
        AddGroundedEntity(GameState, EntityType_Hero, GameState->CameraP, GameState->PlayerCollision);

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

    add_low_entity_result_ Entity =
        AddGroundedEntity(GameState, EntityType_Wall, EntityLowP, GameState->WallCollision);

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides);

    return Entity;
}

internal add_low_entity_result_
AddMonster_(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {

    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity =
        AddGroundedEntity(GameState, EntityType_Monster, EntityLowP, GameState->MonsterCollision);

    InitHitpoints_(Entity.Low, 3);

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides | EntityFlag_Moveable);

    return Entity;
}

internal add_low_entity_result_
AddFamiliar_(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {

    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity =
        AddGroundedEntity(GameState, EntityType_Familiar, EntityLowP, GameState->FamiliarCollision);

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides | EntityFlag_Moveable);

    return Entity;
}

internal add_low_entity_result_
AddStair(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {

    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity =
        AddGroundedEntity(GameState, EntityType_Stairwell, EntityLowP, GameState->StairCollision);

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides);

    Entity.Low->Sim.WalkableHeight = GameState->World->TileDepthInMeters;
    Entity.Low->Sim.WalkableDim = Entity.Low->Sim.Collision->TotalVolume.Dim.XY;

    return Entity;
}


internal add_low_entity_result_ AddStandardRoom(
    game_state* GameState,
    uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ
) {

    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity =
        AddGroundedEntity(GameState, EntityType_Space, EntityLowP, GameState->StandardRoomCollision);

    AddFlags(&Entity.Low->Sim, EntityFlag_Traversable);

    return Entity;
}


// ================================================================================================
