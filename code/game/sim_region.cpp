#include "../types.h"
#include "state.cpp"
#include "world.cpp"

struct sim_entity {
    uint32 StorageIndex;

    v2 P;
    uint32 ChunkZ;

    real32 Z;
    real32 dZ;
};

struct stored_entity {
    world_position P;

    uint32 FacingDirection;
    v2 dP;
    real32 tBob;
};

struct sim_region {
    world* World;

    world_position Origin;
    rectangle2 Bounds;

    uint32 MaxEntityCount;
    uint32 EntityCount;
    sim_entity* Entities;
};

internal v2 GetSimSpaceP(sim_region* SimRegion, stored_entity* Stored) {
    world_difference Diff = Subtract(SimRegion->World, &Stored->P, &SimRegion->Origin);
    return { Diff.d.X, Diff.d.Y };
}

internal sim_entity* AddEntity(sim_region* SimRegion) {
    sim_entity* Entity = 0;
    if (SimRegion->EntityCount < SimRegion->MaxEntityCount) {
        Entity = SimRegion->Entities + SimRegion->EntityCount++;
        *Entity = {};
    } else {
        InvalidCodePath;
    }
    return Entity;
}

internal sim_entity* AddEntity(sim_region* SimRegion, stored_entity* Source, v2* SimP) {
    sim_entity* Dest = AddEntity(SimRegion);
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
                    low_entity* Low = GameState->LowEntities_ + LowEntityIndex;

                    /*v2 SimSpaceP = GetSimSpaceP(SimRegion, Low);
                    if (IsInRectangle(Bounds, SimSpaceP)) {
                        AddEntity(SimRegion, Low, SimSpaceP);
                    }*/

                }
            }
        }
    }

}

internal void EndSim(sim_region* Region, game_state* GameState) {

    sim_entity* Entity = Region->Entities;

    for (uint32 EntityIndex = 0;
        EntityIndex < Region->EntityCount;
        EntityIndex++, Entity++) {

        //! stored_entity
        low_entity* Stored = GameState->LowEntities_ + Entity->StorageIndex;

        world_position NewP = MapIntoChunkSpace(GameState->World, Region->Origin, Entity->P);

        ChangeEntityLocation(
            &GameState->WorldArena, GameState->World, Entity->StorageIndex, Stored,
            &Stored->P, &NewP
        );
    }

#if 0
    entity CameraFollowingEntity = ForceEntityIntoHigh(GameState, GameState->CameraFollowingEntityIndex);
    if (CameraFollowingEntity.High != 0) {

        world_position NewCameraP = GameState->CameraP;

        NewCameraP.ChunkZ = CameraFollowingEntity.Low->P.ChunkZ;

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
        NewCameraP = CameraFollowingEntity.Low->P;
#endif
        SetCamera(GameState, NewCameraP);
#endif
}
