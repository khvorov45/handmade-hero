#include "lib.hpp"
#include "world.cpp"
#include "state.cpp"
#include "random.cpp"
#include "math.cpp"
#include <math.h>

internal void GameOutputSound(game_sound_buffer* SoundBuffer) {
    int16* Samples = SoundBuffer->Samples;
    for (int32 SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex) {
        int16 SampleValue = 0;
        *Samples++ = SampleValue;
        *Samples++ = SampleValue;

    }
}

/// Maximums are not inclusive
internal void DrawRectangle(
    game_offscreen_buffer* Buffer,
    v2 vMin, v2 vMax,
    real32 R, real32 G, real32 B
) {
    int32 MinX = RoundReal32ToInt32(vMin.X);
    int32 MinY = RoundReal32ToInt32(vMin.Y);
    int32 MaxX = RoundReal32ToInt32(vMax.X);
    int32 MaxY = RoundReal32ToInt32(vMax.Y);

    if (MinX < 0) {
        MinX = 0;
    }
    if (MinY < 0) {
        MinY = 0;
    }

    if (MaxX > Buffer->Width) {
        MaxX = Buffer->Width;
    }
    if (MaxY > Buffer->Height) {
        MaxY = Buffer->Height;
    }

    uint32 Color = (RoundReal32ToUint32(R * 255.0f) << 16) |
        (RoundReal32ToUint32(G * 255.0f) << 8) |
        (RoundReal32ToUint32(B * 255.0f));

    uint8* Row = (uint8*)Buffer->Memory + MinY * Buffer->Pitch + MinX * Buffer->BytesPerPixel;
    for (int32 Y = MinY; Y < MaxY; ++Y) {
        uint32* Pixel = (uint32*)Row;
        for (int32 X = MinX; X < MaxX; ++X) {
            *Pixel++ = Color;
        }
        Row += Buffer->Pitch;
    }
}

internal void DrawBitmap(
    game_offscreen_buffer* Buffer, loaded_bitmap* BMP,
    real32 RealStartX, real32 RealStartY,
    real32 CAlpha = 1.0f
) {

    int32 StartX = RoundReal32ToInt32(RealStartX);
    int32 StartY = RoundReal32ToInt32(RealStartY);

    int32 EndX = StartX + BMP->Width;
    int32 EndY = StartY + BMP->Height;

    int32 SourceOffsetX = 0;
    if (StartX < 0) {
        SourceOffsetX = -StartX;
        StartX = 0;
    }

    int32 SourceOffsetY = 0;
    if (StartY < 0) {
        SourceOffsetY = -StartY;
        StartY = 0;
    }

    if (EndX > Buffer->Width) {
        EndX = Buffer->Width;
    }
    if (EndY > Buffer->Height) {
        EndY = Buffer->Height;
    }

    uint32* SourceRow =
        BMP->Pixels + BMP->Width * (BMP->Height - 1) - SourceOffsetY * BMP->Width + SourceOffsetX;
    uint8* DestRow = (uint8*)Buffer->Memory + StartY * Buffer->Pitch + StartX * Buffer->BytesPerPixel;
    for (int32 Y = StartY; Y < EndY; ++Y) {
        uint32* Dest = (uint32*)DestRow;
        uint32* Source = SourceRow;
        for (int32 X = StartX; X < EndX; ++X) {

            real32 A = (real32)((*Source >> 24)) / 255.0f * CAlpha;

            real32 SR = (real32)((*Source >> 16) & 0xFF);
            real32 SG = (real32)((*Source >> 8) & 0xFF);
            real32 SB = (real32)((*Source) & 0xFF);

            real32 DR = (real32)((*Dest >> 16) & 0xFF);
            real32 DG = (real32)((*Dest >> 8) & 0xFF);
            real32 DB = (real32)((*Dest) & 0xFF);

            real32 RR = (1.0f - A) * DR + A * SR;
            real32 RG = (1.0f - A) * DG + A * SG;
            real32 RB = (1.0f - A) * DB + A * SB;

            *Dest = (RoundReal32ToUint32(RR) << 16) | (RoundReal32ToUint32(RG) << 8) | (RoundReal32ToUint32(RB));

            ++Dest;
            ++Source;
        }
        DestRow += Buffer->Pitch;
        SourceRow -= BMP->Width;
    }
}

internal v2 GetCameraSpaceP(game_state* GameState, low_entity* EntityLow) {
    world_difference Diff = Subtract(GameState->World, &EntityLow->P, &GameState->CameraP);
    return { Diff.d.X, Diff.d.Y };
}

internal high_entity* MakeEntityHigFrequency(
    game_state* GameState, low_entity* EntityLow, uint32 LowIndex, v2 CameraSpaceP
) {
    high_entity* EntityHigh = 0;

    Assert(EntityLow->HighEntityIndex == 0);

    if (GameState->HighEntityCount < ArrayCount(GameState->HighEntities_)) {
        uint32 HighIndex = GameState->HighEntityCount++;
        EntityHigh = &GameState->HighEntities_[HighIndex];

        EntityHigh->P = CameraSpaceP;
        EntityHigh->dP = { 0.0f, 0.0f };
        EntityHigh->ChunkZ = EntityLow->P.ChunkZ;
        EntityHigh->FacingDirection = 0;

        EntityHigh->LowEntityIndex = LowIndex;

        EntityLow->HighEntityIndex = HighIndex;
    } else {
        InvalidCodePath
    }

    return EntityHigh;
}

internal high_entity* MakeEntityHigFrequency(game_state* GameState, uint32 LowIndex) {

    low_entity* EntityLow = GameState->LowEntities_ + LowIndex;

    high_entity* EntityHigh = 0;

    if (EntityLow->HighEntityIndex) {
        EntityHigh = &GameState->HighEntities_[EntityLow->HighEntityIndex];
    } else {
        v2 CameraSpaceP = GetCameraSpaceP(GameState, EntityLow);
        EntityHigh = MakeEntityHigFrequency(GameState, EntityLow, LowIndex, CameraSpaceP);
    }

    return EntityHigh;
}

internal void MakeEntityLowFrequency(game_state* GameState, uint32 LowIndex) {
    low_entity* EntityLow = &GameState->LowEntities_[LowIndex];
    uint32 HighIndex = EntityLow->HighEntityIndex;

    if (HighIndex != 0) {
        uint32 LastHighIndex = GameState->HighEntityCount - 1;
        if (HighIndex != LastHighIndex) {
            high_entity* LastHighEntity = &GameState->HighEntities_[LastHighIndex];
            high_entity* DeletedEntity = &GameState->HighEntities_[HighIndex];
            *DeletedEntity = *LastHighEntity;
            GameState->LowEntities_[LastHighEntity->LowEntityIndex].HighEntityIndex = HighIndex;
        }
        --GameState->HighEntityCount;
        EntityLow->HighEntityIndex = 0;
    }
}

internal bool32 ValidateEntityPairs(game_state* GameState) {
    bool32 Valid = true;
    for (uint32 HighEntityIndex = 1;
        HighEntityIndex < GameState->HighEntityCount;
        HighEntityIndex++) {

        high_entity* High = GameState->HighEntities_ + HighEntityIndex;
        Valid = Valid &&
            GameState->LowEntities_[High->LowEntityIndex].HighEntityIndex == HighEntityIndex;
    }
    return Valid;
}

internal inline void OffsetAndCheckFrequencyByArea(
    game_state* GameState, v2 Offset, rectangle2 HighFrequencyBounds
) {
    for (uint32 HighEntityIndex = 1; HighEntityIndex < GameState->HighEntityCount;) {
        high_entity* High = GameState->HighEntities_ + HighEntityIndex;
        High->P += Offset;
        low_entity* Low = GameState->LowEntities_ + High->LowEntityIndex;
        if (IsValid(Low->P) && IsInRectangle(HighFrequencyBounds, High->P)) {
            HighEntityIndex++;
        } else {
            Assert(
                GameState->LowEntities_[High->LowEntityIndex].HighEntityIndex == HighEntityIndex
            );
            MakeEntityLowFrequency(GameState, High->LowEntityIndex);
        }
    }
}

internal low_entity* GetLowEntity(game_state* GameState, uint32 LowIndex) {

    low_entity* Result = 0;

    if (LowIndex > 0 && LowIndex < GameState->LowEntityCount) {
        Result = GameState->LowEntities_ + LowIndex;
    }

    return Result;
}

internal entity ForceEntityIntoHigh(game_state* GameState, uint32 LowIndex) {

    entity Result = {};

    if (LowIndex > 0 && LowIndex < GameState->LowEntityCount) {
        Result.LowIndex = LowIndex;
        Result.Low = GameState->LowEntities_ + LowIndex;
        Result.High = MakeEntityHigFrequency(GameState, LowIndex);
    }

    return Result;
}

struct add_low_entity_result {
    low_entity* Low;
    uint32 LowIndex;
};

internal add_low_entity_result
AddLowEntity(game_state* GameState, entity_type EntityType, world_position* Position) {

    Assert(GameState->LowEntityCount < ArrayCount(GameState->LowEntities_));

    add_low_entity_result Result = {};

    Result.LowIndex = GameState->LowEntityCount++;

    Result.Low = GameState->LowEntities_ + Result.LowIndex;
    *Result.Low = {};
    Result.Low->Type = EntityType;

    ChangeEntityLocation(
        &GameState->WorldArena, GameState->World, Result.LowIndex, Result.Low, 0, Position
    );


    return Result;
}

internal void InitHitpoints(low_entity* EntityLow, uint32 Max) {
    Assert(Max <= ArrayCount(EntityLow->HitPoint));
    EntityLow->HitPointMax = Max;
    for (uint32 HealthIndex = 0; HealthIndex < Max; HealthIndex++) {
        EntityLow->HitPoint[HealthIndex].FilledAmount = HITPOINT_SUBCOUNT;
    }
}

internal add_low_entity_result AddSword(game_state* GameState) {

    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Sword, 0);

    Entity.Low->Height = 0.5f;
    Entity.Low->Width = 1.0f;


    Entity.Low->Collides = false;

    return Entity;
}

internal add_low_entity_result AddPlayer(game_state* GameState) {

    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Hero, &GameState->CameraP);

    Entity.Low->Height = 0.5f;
    Entity.Low->Width = 1.0f;

    Entity.Low->Collides = true;

    InitHitpoints(Entity.Low, 3);

    add_low_entity_result Sword = AddSword(GameState);
    Entity.Low->SwordLowIndex = Sword.LowIndex;

    if (GameState->CameraFollowingEntityIndex == 0) {
        GameState->CameraFollowingEntityIndex = Entity.LowIndex;
    }

    return Entity;
}

internal add_low_entity_result
AddWall(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Wall, &EntityLowP);

    real32 TileSide = GameState->World->TileSideInMeters;

    Entity.Low->Height = TileSide;
    Entity.Low->Width = TileSide;

    Entity.Low->Collides = true;

    return Entity;
}

internal add_low_entity_result
AddMonster(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Monster, &EntityLowP);

    Entity.Low->Height = 0.5f;
    Entity.Low->Width = 1.0f;

    InitHitpoints(Entity.Low, 3);

    Entity.Low->Collides = true;

    return Entity;
}

internal add_low_entity_result
AddFamiliar(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Familiar, &EntityLowP);

    Entity.Low->Height = 0.5f;
    Entity.Low->Width = 1.0f;

    Entity.Low->Collides = false;

    return Entity;
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
MoveEntity(game_state* GameState, entity Entity, real32 dt, move_spec* MoveSpec, v2 ddPlayer) {

    world* TileMap = GameState->World;

    if (MoveSpec->UnitMaxAccelVector) {
        real32 ddPlayerLengthSq = LengthSq(ddPlayer);
        if (ddPlayerLengthSq > 1.0f) {
            ddPlayer *= 1.0f / SquareRoot(ddPlayerLengthSq);
        }
    }

    ddPlayer *= MoveSpec->Speed;

    ddPlayer += -MoveSpec->Drag * Entity.High->dP;

    v2 OldPlayerP = Entity.High->P;

    v2 PlayerDelta = 0.5f * ddPlayer * Square(dt) + Entity.High->dP * dt;

    v2 NewPlayerP = OldPlayerP + PlayerDelta;
    Entity.High->dP += ddPlayer * dt;

    for (uint32 Iteration = 0; Iteration < 4; ++Iteration) {

        v2 DesiredPosition = Entity.High->P + PlayerDelta;

        real32 tMin = 1.0f;
        v2 WallNormal = {};
        uint32 HitHighEntityIndex = 0;

        if (Entity.Low->Collides) {

            for (uint32 TestHighEntityIndex = 1; TestHighEntityIndex < GameState->HighEntityCount; ++TestHighEntityIndex) {

                if (TestHighEntityIndex == Entity.Low->HighEntityIndex) {
                    continue;
                }

                entity TestEntity = {};
                TestEntity.High = GameState->HighEntities_ + TestHighEntityIndex;
                TestEntity.LowIndex = TestEntity.High->LowEntityIndex;
                TestEntity.Low = GameState->LowEntities_ + TestEntity.LowIndex;

                if (!TestEntity.Low->Collides) {
                    continue;
                }

                real32 DiameterW = Entity.Low->Width + TestEntity.Low->Width;
                real32 DiameterH = Entity.Low->Height + TestEntity.Low->Height;

                v2 MinCorner = -0.5f * v2{ DiameterW, DiameterH };
                v2 MaxCorner = 0.5f * v2{ DiameterW, DiameterH };

                v2 Rel = Entity.High->P - TestEntity.High->P;

                if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y)) {
                    WallNormal = { -1, 0 };
                    HitHighEntityIndex = TestHighEntityIndex;
                }
                if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y)) {
                    WallNormal = { 1, 0 };
                    HitHighEntityIndex = TestHighEntityIndex;
                }
                if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X)) {
                    WallNormal = { 0, -1 };
                    HitHighEntityIndex = TestHighEntityIndex;
                }
                if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X)) {
                    WallNormal = { 0, 1 };
                    HitHighEntityIndex = TestHighEntityIndex;
                }
            }
        }

        Entity.High->P += tMin * PlayerDelta;
        if (HitHighEntityIndex != 0) {
            Entity.High->dP = Entity.High->dP - Inner(Entity.High->dP, WallNormal) * WallNormal;
            PlayerDelta = DesiredPosition - Entity.High->P;
            PlayerDelta = PlayerDelta - Inner(PlayerDelta, WallNormal) * WallNormal;

            high_entity* HitHigh = GameState->HighEntities_ + HitHighEntityIndex;
            low_entity* HitLow = GameState->LowEntities_ + HitHigh->LowEntityIndex;
            //HitHigh->AbsTileZ += HitLow->dAbsTileZ;
        } else {
            break;
        }

    }

    if (AbsoluteValue(Entity.High->dP.Y) > AbsoluteValue(Entity.High->dP.X)) {
        if (Entity.High->dP.Y > 0) {
            Entity.High->FacingDirection = 1;
        } else {
            Entity.High->FacingDirection = 3;
        }
    } else if (AbsoluteValue(Entity.High->dP.Y) < AbsoluteValue(Entity.High->dP.X)) {
        if (Entity.High->dP.X > 0) {
            Entity.High->FacingDirection = 0;
        } else {
            Entity.High->FacingDirection = 2;
        }
    }

    world_position NewWorldP =
        MapIntoChunkSpace(GameState->World, GameState->CameraP, Entity.High->P);
    ChangeEntityLocation(
        &GameState->WorldArena, GameState->World, Entity.LowIndex, Entity.Low,
        &Entity.Low->P, &NewWorldP
    );
}

internal void SetCamera(game_state* GameState, world_position NewCameraP) {

    Assert(ValidateEntityPairs(GameState));

    world* TileMap = GameState->World;
    world_difference dCameraP = Subtract(TileMap, &NewCameraP, &GameState->CameraP);

    GameState->CameraP = NewCameraP;

    uint32 TileSpanX = 17 * 3;
    uint32 TileSpanY = 9 * 3;

    rectangle2 CameraBounds = RectCenterDim(
        { 0, 0 },
        TileMap->TileSideInMeters * v2{ (real32)TileSpanX, (real32)TileSpanY }
    );

    v2 EntityOffsetForFrame = -v2{ dCameraP.d.X, dCameraP.d.Y };
    OffsetAndCheckFrequencyByArea(GameState, EntityOffsetForFrame, CameraBounds);

    Assert(ValidateEntityPairs(GameState));

    world_position MinChunkP =
        MapIntoChunkSpace(GameState->World, NewCameraP, GetMinCorner(CameraBounds));
    world_position MaxChunkP =
        MapIntoChunkSpace(GameState->World, NewCameraP, GetMaxCorner(CameraBounds));

    for (int32 ChunkY = MinChunkP.ChunkY; ChunkY <= MaxChunkP.ChunkY; ChunkY++) {

        for (int32 ChunkX = MinChunkP.ChunkX; ChunkX <= MaxChunkP.ChunkX; ChunkX++) {

            world_chunk* Chunk = GetChunk(GameState->World, ChunkX, ChunkY, NewCameraP.ChunkZ);
            if (Chunk == 0) {
                continue;
            }

            for (world_entity_block* Block = &Chunk->FirstBlock; Block; Block = Block->Next) {

                for (uint32 BlockIndex = 0; BlockIndex < Block->EntityCount; ++BlockIndex) {
                    uint32 LowEntityIndex = Block->LowEntityIndex[BlockIndex];
                    low_entity* Low = GameState->LowEntities_ + LowEntityIndex;

                    if (Low->HighEntityIndex == 0) {

                        v2 CameraSpaceP = GetCameraSpaceP(GameState, Low);

                        if (IsInRectangle(CameraBounds, CameraSpaceP)) {
                            MakeEntityHigFrequency(GameState, Low, LowEntityIndex, CameraSpaceP);
                        }
                    }
                }
            }
        }
    }

    Assert(ValidateEntityPairs(GameState));
}

internal entity EntityFromHighIndex(game_state* GameState, uint32 HighEntityIndex) {
    entity Result = {};
    if (HighEntityIndex == 0) {
        return Result;
    }
    Assert(HighEntityIndex < GameState->HighEntityCount);
    Result.High = GameState->HighEntities_ + HighEntityIndex;
    Result.LowIndex = Result.High->LowEntityIndex;
    Result.Low = GameState->LowEntities_ + Result.LowIndex;
    return Result;
}

internal void UpdateFamiliar(game_state* GameState, entity Entity, real32 dt) {
    entity ClosestHero = {};
    real32 ClosestHeroDSq = Square(15.0f);
    for (uint32 HighEntityIndex = 1;
        HighEntityIndex < GameState->HighEntityCount;
        HighEntityIndex++) {

        entity TestEntity = EntityFromHighIndex(GameState, HighEntityIndex);

        if (TestEntity.Low->Type == EntityType_Hero) {
            real32 TestDSq = LengthSq(TestEntity.High->P - Entity.High->P);
            if (TestDSq < ClosestHeroDSq) {
                ClosestHeroDSq = TestDSq;
                ClosestHero = TestEntity;
            }
        }
    }
    v2 ddP = {};
    if (ClosestHero.High && ClosestHeroDSq > Square(3.0f)) {
        real32 Acceleration = 0.3f;
        real32 OneOverLength = (Acceleration / SquareRoot(ClosestHeroDSq));
        ddP = (ClosestHero.High->P - Entity.High->P) * OneOverLength;
    }
    move_spec MoveSpec = DefaultMoveSpec();
    MoveSpec.Drag = 8.0f;
    MoveSpec.Speed = 150.0f;
    MoveSpec.UnitMaxAccelVector = true;
    MoveEntity(GameState, Entity, dt, &MoveSpec, ddP);
}

internal void UpdateMonster(game_state* GameState, entity Entity, real32 dt) {}

internal void UpdateSword(game_state* GameState, entity Entity, real32 dt) {
    move_spec MoveSpec = DefaultMoveSpec();
    MoveSpec.Speed = 0.0f;
    MoveSpec.Drag = 0.0f;
    MoveSpec.UnitMaxAccelVector = false;

    v2 OldP = Entity.High->P;
    MoveEntity(GameState, Entity, dt, &MoveSpec, { 0.0f, 0.0f });
    real32 DistanceTravelled = Length(OldP - Entity.High->P);

    Entity.Low->DistanceRemaining -= DistanceTravelled;
    if (Entity.Low->DistanceRemaining < 0.0f) {
        ChangeEntityLocation(
            &GameState->WorldArena, GameState->World, Entity.LowIndex, Entity.Low, &Entity.Low->P, 0
        );
    }
}

internal inline void
PushPiece(
    entity_visible_piece_group* Group, loaded_bitmap* Bitmap,
    v2 Offset, real32 OffsetZ, v2 Align, v2 Dim,
    v4 Color,
    real32 EntityZC = 1.0f
) {
    Assert(Group->PieceCount < ArrayCount(Group->Pieces));
    real32 MetersToPixels = Group->GameState->MetersToPixels;
    entity_visible_piece* Piece = Group->Pieces + Group->PieceCount++;

    v2 OffsetFixY = { Offset.X, -Offset.Y };

    Piece->R = Color.R;
    Piece->G = Color.G;
    Piece->B = Color.B;
    Piece->A = Color.A;
    Piece->Bitmap = Bitmap;
    Piece->Offset = MetersToPixels * OffsetFixY - Align;
    Piece->OffsetZ = MetersToPixels * OffsetZ;
    Piece->EntityZC = EntityZC;
    Piece->Dim = Dim;
}

internal inline void
PushBitmap(
    entity_visible_piece_group* Group, loaded_bitmap* Bitmap,
    v2 Offset, real32 OffsetZ, v2 Align, real32 Alpha = 1.0f,
    real32 EntityZC = 1.0f
) {
    PushPiece(
        Group, Bitmap, Offset, OffsetZ, Align, { 0, 0 }, { 0, 0, 0, Alpha }, EntityZC
    );
}

internal void
PushRect(
    entity_visible_piece_group* Group, loaded_bitmap* Bitmap,
    v2 Offset, real32 OffsetZ, v2 Dim,
    v4 Color,
    real32 EntityZC = 1.0f
) {
    PushPiece(
        Group, 0, Offset, OffsetZ, { 0, 0 }, Dim, Color, EntityZC
    );
}

internal void DrawHitpoints(low_entity* LowEntity, entity_visible_piece_group* PieceGroup) {
    if (LowEntity->HitPointMax >= 1) {
        v2 HealthDim = { 0.2f, 0.2f };
        real32 SpacingX = 2.0f * HealthDim.X;
        v2 HitP = { (LowEntity->HitPointMax - 1) * (-0.5f) * SpacingX, -0.2f };
        v2 dHitP = { SpacingX, 0.0f };
        for (uint32 HealthIndex = 0; HealthIndex < LowEntity->HitPointMax; ++HealthIndex) {
            v4 Color = { 1.0f, 0.0f, 0.0f, 1.0f };
            hit_point* HitPoint = LowEntity->HitPoint + HealthIndex;
            if (HitPoint->FilledAmount == 0) {
                Color.R = 0.2f;
                Color.G = 0.2f;
                Color.B = 0.2f;
            }
            PushRect(PieceGroup, 0, HitP, 0, HealthDim, Color, 0.0f);
            HitP += dHitP;
        }
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(SoundBuffer);
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    Assert(
        &Input->Controllers[0].Terminator - &Input->Controllers[0].MoveUp ==
        ArrayCount(Input->Controllers[0].Buttons)
    );

    game_state* GameState = (game_state*)Memory->PermanentStorage;

    if (!Memory->IsInitialized) {
        AddLowEntity(GameState, EntityType_Null, 0);
        GameState->HighEntityCount = 1;

        GameState->Backdrop =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");
        GameState->HeroShadow =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_shadow.bmp");
        GameState->Tree =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/tree00.bmp");
        GameState->Sword =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/rock03.bmp");

        hero_bitmaps* Bitmap;

        Bitmap = &GameState->HeroBitmaps[0];
        Bitmap->Head =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_head.bmp");
        Bitmap->Cape =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_cape.bmp");
        Bitmap->Torso =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_torso.bmp");
        Bitmap->Align = { 72, 182 };

        ++Bitmap;
        Bitmap->Head =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_head.bmp");
        Bitmap->Cape =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_cape.bmp");
        Bitmap->Torso =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_torso.bmp");
        Bitmap->Align = { 72, 182 };

        ++Bitmap;
        Bitmap->Head =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_head.bmp");
        Bitmap->Cape =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_cape.bmp");
        Bitmap->Torso =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_torso.bmp");
        Bitmap->Align = { 72, 182 };

        ++Bitmap;
        Bitmap->Head =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_head.bmp");
        Bitmap->Cape =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_cape.bmp");
        Bitmap->Torso =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_torso.bmp");
        Bitmap->Align = { 72, 182 };

        InitializeArena(
            &GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
            (uint8*)Memory->PermanentStorage + sizeof(game_state)
        );

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world* TileMap = GameState->World;
        InitializeWorld(TileMap, 1.4f);

        uint32 TileSideInPixels = 60;
        GameState->MetersToPixels = (real32)TileSideInPixels / (real32)TileMap->TileSideInMeters;

        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;

        uint32 ScreenBaseX = 0;
        uint32 ScreenBaseY = 0;
        uint32 ScreenBaseZ = 0;

        uint32 ScreenX = ScreenBaseX;
        uint32 ScreenY = ScreenBaseY;
        uint32 AbsTileZ = ScreenBaseZ;

        uint32 RandomNumberIndex = 0;

        bool32 DoorLeft = false;
        bool32 DoorRight = false;
        bool32 DoorTop = false;
        bool32 DoorBottom = false;
        bool32 DoorUp = false;
        bool32 DoorDown = false;

        for (uint32 ScreenIndex = 0; ScreenIndex < 2000; ++ScreenIndex) {

            Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));

            uint32 RandomChoice;
            if (1) { //DoorUp || DoorDown) {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
            } else {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
            }

            bool32 CreatedZDoor = false;

            if (RandomChoice == 2) {
                CreatedZDoor = true;
                if (AbsTileZ == ScreenBaseZ) {
                    DoorUp = true;
                } else {
                    DoorDown = true;
                }
            } else if (RandomChoice == 1) {
                DoorRight = true;
            } else {
                DoorTop = true;
            }

            for (uint32 TileY = 0; TileY < TilesPerHeight; ++TileY) {
                for (uint32 TileX = 0; TileX < TilesPerWidth; ++TileX) {
                    uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;

                    uint32 TileValue = 1;
                    if (TileX == 0 && (!DoorLeft || TileY != TilesPerHeight / 2)) {
                        TileValue = 2;
                    }
                    if (TileX == TilesPerWidth - 1 && (!DoorRight || TileY != TilesPerHeight / 2)) {
                        TileValue = 2;
                    }
                    if (TileY == 0 && (!DoorBottom || TileX != TilesPerWidth / 2)) {
                        TileValue = 2;
                    }
                    if (TileY == TilesPerHeight - 1 && (!DoorTop || TileX != TilesPerWidth / 2)) {
                        TileValue = 2;
                    }
                    if (TileX == 10 && TileY == 6) {
                        if (DoorUp) {
                            TileValue = 3;
                        }
                        if (DoorDown) {
                            TileValue = 4;
                        }
                    }

                    if (TileValue == 2) {
                        AddWall(GameState, AbsTileX, AbsTileY, AbsTileZ);
                    }
                }
            }

            DoorLeft = DoorRight;
            DoorBottom = DoorTop;

            DoorRight = false;
            DoorTop = false;

            if (CreatedZDoor) {
                DoorUp = !DoorUp;
                DoorDown = !DoorDown;
            } else {
                DoorUp = false;
                DoorDown = false;
            }

            if (RandomChoice == 2) {
                if (AbsTileZ == ScreenBaseZ) {
                    AbsTileZ = ScreenBaseZ + 1;
                } else {
                    AbsTileZ = ScreenBaseZ;
                }
            } else if (RandomChoice == 1) {
                ScreenX += 1;
            } else {
                ScreenY += 1;
            }

        }

#if 0
        while (GameState->LowEntityCount < (ArrayCount(GameState->LowEntities_) - 16)) {
            uint32 Coordinate = 1024 + GameState->LowEntityCount;
            AddWall(GameState, Coordinate, Coordinate, Coordinate);
        }
#endif

        uint32 CameraTileX = ScreenBaseX * TilesPerWidth + 17 / 2;
        uint32 CameraTileY = ScreenBaseY * TilesPerHeight + 9 / 2;
        uint32 CameraTileZ = ScreenBaseZ;

        AddMonster(GameState, CameraTileX + 2, CameraTileY + 2, CameraTileZ);
        AddFamiliar(GameState, CameraTileX - 2, CameraTileY - 2, CameraTileZ);

        world_position NewCameraP = ChunkPositionFromTilePosition(
            GameState->World,
            CameraTileX,
            CameraTileY,
            CameraTileZ
        );
        SetCamera(GameState, NewCameraP);

        Memory->IsInitialized = true;
    }

    world* World = GameState->World;
    world* TileMap = World;

    for (int32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex) {

        game_controller_input* Controller = GetController(Input, ControllerIndex);

        uint32 LowIndex = GameState->PlayerIndexForController[ControllerIndex];
        if (LowIndex == 0) {
            if (Controller->Start.EndedDown) {
                add_low_entity_result PlayerEntity = AddPlayer(GameState);
                GameState->PlayerIndexForController[ControllerIndex] = PlayerEntity.LowIndex;
            }
        } else {

            entity ControllingEntity = ForceEntityIntoHigh(GameState, LowIndex);

            v2 ddPlayer = {};

            if (Controller->IsAnalog) {
                ddPlayer = { Controller->StickAverageX, Controller->StickAverageY };
            } else {
                if (Controller->MoveUp.EndedDown) {
                    ddPlayer.Y = 1;
                }
                if (Controller->MoveDown.EndedDown) {
                    ddPlayer.Y = -1;
                }
                if (Controller->MoveLeft.EndedDown) {
                    ddPlayer.X = -1;
                }
                if (Controller->MoveRight.EndedDown) {
                    ddPlayer.X = 1;
                }
            }

            if (Controller->Start.EndedDown) {
                ControllingEntity.High->dZ = 3.0f;
            }

            v2 dSword = {};
            if (Controller->ActionUp.EndedDown) {
                dSword = { 0.0f, 1.0f };
            }
            if (Controller->ActionDown.EndedDown) {
                dSword = { 0.0f, -1.0f };
            }
            if (Controller->ActionLeft.EndedDown) {
                dSword = { -1.0f, 0.0f };
            }
            if (Controller->ActionRight.EndedDown) {
                dSword = { 1.0f, 0.0f };
            }

            move_spec MoveSpec = DefaultMoveSpec();
            MoveSpec.Drag = 8.0f;
            MoveSpec.Speed = 150.0f;
            MoveSpec.UnitMaxAccelVector = true;
            MoveEntity(GameState, ControllingEntity, Input->dtForFrame, &MoveSpec, ddPlayer);

            if (dSword.X != 0.0f || dSword.Y != 0.0f) {
                uint32 SwordIndex = ControllingEntity.Low->SwordLowIndex;
                low_entity* LowSword = GetLowEntity(GameState, SwordIndex);
                if (LowSword && !IsValid(LowSword->P)) {
                    world_position SwordP = ControllingEntity.Low->P;
                    ChangeEntityLocation(
                        &GameState->WorldArena, GameState->World,
                        SwordIndex, LowSword, 0, &SwordP
                    );

                    entity Sword = ForceEntityIntoHigh(GameState, SwordIndex);

                    Sword.Low->DistanceRemaining = 5.0f;
                    Sword.High->dP = 15.0f * dSword;
                }
            }
        }
    }

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
    }

    //* Clear screen
    DrawRectangle(
        Buffer, { 0, 0 }, { (real32)Buffer->Width, (real32)Buffer->Height },
        1.0f, 0.0f, 1.0f
    );

    //* Draw backdrop
#if 0
    DrawBitmap(Buffer, GameState->Backdrop, 0.0f, 0.0f);
#else
    DrawRectangle(
        Buffer, { 0, 0 }, { (real32)Buffer->Width, (real32)Buffer->Height },
        0.5f, 0.5f, 0.5f
    );
#endif

    //* Draw entities
    real32 ScreenCenterX = (real32)Buffer->Width * 0.5f;
    real32 ScreenCenterY = (real32)Buffer->Height * 0.5f;
    entity_visible_piece_group PieceGroup;
    PieceGroup.GameState = GameState;
    for (uint32 HighEntityIndex = 1; HighEntityIndex < GameState->HighEntityCount; HighEntityIndex++) {
        high_entity* HighEntity = GameState->HighEntities_ + HighEntityIndex;
        low_entity* LowEntity = GameState->LowEntities_ + HighEntity->LowEntityIndex;

        entity Entity = {};
        Entity.LowIndex = HighEntity->LowEntityIndex;
        Entity.Low = LowEntity;
        Entity.High = HighEntity;

        real32 dt = Input->dtForFrame;

        real32 ShadowAlpha = Maximum(0.0f, 1.0f - HighEntity->Z);

        hero_bitmaps* HeroBitmaps = &GameState->HeroBitmaps[HighEntity->FacingDirection];
        PieceGroup.PieceCount = 0;
        switch (LowEntity->Type) {
        case EntityType_Hero:
        {
            PushBitmap(&PieceGroup, &GameState->HeroShadow, { 0, 0 }, 0, HeroBitmaps->Align, ShadowAlpha, 0.0f);
            PushBitmap(&PieceGroup, &HeroBitmaps->Head, { 0, 0 }, 0, HeroBitmaps->Align);
            PushBitmap(&PieceGroup, &HeroBitmaps->Torso, { 0, 0 }, 0, HeroBitmaps->Align);
            PushBitmap(&PieceGroup, &HeroBitmaps->Cape, { 0, 0 }, 0, HeroBitmaps->Align);

            DrawHitpoints(LowEntity, &PieceGroup);
        }
        break;
        case EntityType_Wall:
        {
            PushBitmap(&PieceGroup, &GameState->Tree, { 0, 0 }, 0, { 40, 80 });
        }
        break;
        case EntityType_Sword:
        {
            UpdateSword(GameState, Entity, dt);
            PushBitmap(&PieceGroup, &GameState->HeroShadow, { 0, 0 }, 0, HeroBitmaps->Align, ShadowAlpha, 0.0f);
            PushBitmap(&PieceGroup, &GameState->Sword, { 0, 0 }, 0, { 29, 10 });
        }
        break;
        case EntityType_Familiar:
        {
            Entity.High->tBob += dt;
            real32 Pi2 = 2 * Pi32;
            if (Entity.High->tBob > Pi2) {
                Entity.High->tBob -= Pi2;
            }
            real32 BobSin = Sin(4.0f * Entity.High->tBob);
            UpdateFamiliar(GameState, Entity, dt);
            PushBitmap(&PieceGroup, &GameState->HeroShadow, { 0, 0 }, 0, HeroBitmaps->Align, ShadowAlpha * 0.5f + BobSin * 0.2f);
            PushBitmap(&PieceGroup, &HeroBitmaps->Head, { 0, 0 }, 0.2f * BobSin, HeroBitmaps->Align);
        }
        break;
        case EntityType_Monster:
        {
            UpdateMonster(GameState, Entity, dt);
            PushBitmap(&PieceGroup, &GameState->HeroShadow, { 0, 0 }, 0, HeroBitmaps->Align, ShadowAlpha);
            PushBitmap(&PieceGroup, &HeroBitmaps->Torso, { 0, 0 }, 0, HeroBitmaps->Align);

            DrawHitpoints(LowEntity, &PieceGroup);
        }
        break;
        default:
        {
            InvalidCodePath;
        }
        break;
        }

        real32 ddZ = -9.8f;
        HighEntity->Z += 0.5f * ddZ * Square(dt) + HighEntity->dZ * dt;
        HighEntity->dZ = ddZ * dt + HighEntity->dZ;
        if (HighEntity->Z < 0) {
            HighEntity->Z = 0;
        }
        real32 Z = -HighEntity->Z * GameState->MetersToPixels;
        real32 EntityGroudX = ScreenCenterX + HighEntity->P.X * GameState->MetersToPixels;
        real32 EntityGroudY = ScreenCenterY - HighEntity->P.Y * GameState->MetersToPixels;

        for (uint32 PieceIndex = 0; PieceIndex < PieceGroup.PieceCount; ++PieceIndex) {
            entity_visible_piece* Piece = PieceGroup.Pieces + PieceIndex;
            v2 Center = {
                EntityGroudX + Piece->Offset.X,
                EntityGroudY + Piece->Offset.Y + Piece->OffsetZ + Z * Piece->EntityZC
            };
            if (Piece->Bitmap) {
                DrawBitmap(
                    Buffer,
                    Piece->Bitmap,
                    Center.X, Center.Y,
                    Piece->A
                );
            } else {
                v2 HalfDim = GameState->MetersToPixels * Piece->Dim * 0.5f;
                DrawRectangle(Buffer, Center - HalfDim, Center + HalfDim, Piece->R, Piece->G, Piece->B);
            }
        }
    }
}
