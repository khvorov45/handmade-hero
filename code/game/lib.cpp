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

internal void DrawBMP(
    game_offscreen_buffer* Buffer, loaded_bmp BMP,
    real32 RealStartX, real32 RealStartY,
    int32 AlignX = 0, int32 AlignY = 0,
    real32 CAlpha = 1.0f
) {
    RealStartX -= (real32)AlignX;
    RealStartY -= (real32)AlignY;

    int32 StartX = RoundReal32ToInt32(RealStartX);
    int32 StartY = RoundReal32ToInt32(RealStartY);

    int32 EndX = StartX + BMP.Width;
    int32 EndY = StartY + BMP.Height;

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

    uint32* SourceRow = BMP.Pixels + BMP.Width * (BMP.Height - 1) - SourceOffsetY * BMP.Width + SourceOffsetX;
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
        SourceRow -= BMP.Width;
    }
}

internal high_entity* MakeEntityHigFrequency(game_state* GameState, uint32 LowIndex) {

    high_entity* EntityHigh = 0;
    low_entity* EntityLow = &GameState->LowEntities_[LowIndex];

    if (EntityLow->HighEntityIndex == 0) {
        if (GameState->HighEntityCount < ArrayCount(GameState->HighEntities_)) {
            uint32 HighIndex = GameState->HighEntityCount++;
            EntityHigh = &GameState->HighEntities_[HighIndex];

            world_difference Diff =
                Subtract(GameState->World, &EntityLow->P, &GameState->CameraP);
            EntityHigh->P = Diff.dXY;
            EntityHigh->dP = { 0.0f, 0.0f };
            EntityHigh->AbsTileZ = EntityLow->P.AbsTileZ;
            EntityHigh->FacingDirection = 0;

            EntityHigh->LowEntityIndex = LowIndex;

            EntityLow->HighEntityIndex = HighIndex;
        } else {
            InvalidCodePath
        }
    } else {
        EntityHigh = &GameState->HighEntities_[EntityLow->HighEntityIndex];
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

internal inline void OffsetAndCheckFrequencyByArea(
    game_state* GameState, v2 Offset, rectangle2 HighFrequencyBounds
) {
    for (uint32 EntityIndex = 1; EntityIndex < GameState->HighEntityCount;) {
        high_entity* High = GameState->HighEntities_ + EntityIndex;
        High->P += Offset;
        if (IsInRectangle(HighFrequencyBounds, High->P)) {
            EntityIndex++;
        } else {
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

internal entity GetHighEntity(game_state* GameState, uint32 LowIndex) {

    entity Result = {};

    if (LowIndex > 0 && LowIndex < GameState->LowEntityCount) {
        Result.LowIndex = LowIndex;
        Result.Low = GameState->LowEntities_ + LowIndex;
        Result.High = MakeEntityHigFrequency(GameState, LowIndex);
    }

    return Result;
}

internal uint32 AddLowEntity(game_state* GameState, entity_type EntityType) {

    Assert(GameState->LowEntityCount < ArrayCount(GameState->LowEntities_));

    uint32 EntityIndex = GameState->LowEntityCount++;

    GameState->LowEntities_[EntityIndex] = {};
    GameState->LowEntities_[EntityIndex].Type = EntityType;

    return EntityIndex;
}

internal uint32 AddPlayer(game_state* GameState) {

    uint32 EntityIndex = AddLowEntity(GameState, EntityType_Hero);

    low_entity* EntityLow = GetLowEntity(GameState, EntityIndex);

    EntityLow->P = GameState->CameraP;
    EntityLow->P.Offset_.X = 0.0f;
    EntityLow->P.Offset_.Y = 0.0f;

    EntityLow->Height = 0.5f;
    EntityLow->Width = 1.0f;

    EntityLow->Collides = true;

    if (GameState->CameraFollowingEntityIndex == 0) {
        GameState->CameraFollowingEntityIndex = EntityIndex;
    }

    return EntityIndex;
}

internal uint32 AddWall(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    uint32 EntityIndex = AddLowEntity(GameState, EntityType_Wall);

    low_entity* EntityLow = GetLowEntity(GameState, EntityIndex);

    EntityLow->P.AbsTileX = AbsTileX;
    EntityLow->P.AbsTileY = AbsTileY;
    EntityLow->P.AbsTileZ = AbsTileZ;

    real32 TileSide = GameState->World->TileSideInMeters;

    EntityLow->Height = TileSide;
    EntityLow->Width = TileSide;

    EntityLow->Collides = true;

    return EntityIndex;
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


internal void MovePlayer(game_state* GameState, entity Entity, real32 dt, v2 ddPlayer) {

    world* TileMap = GameState->World;

    real32 ddPlayerLengthSq = LengthSq(ddPlayer);
    if (ddPlayerLengthSq > 1.0f) {
        ddPlayer *= 1.0f / SquareRoot(ddPlayerLengthSq);
    }

    real32 PlayerAcceleration = 150.0f;

    ddPlayer *= PlayerAcceleration;

    ddPlayer += -8.0f * Entity.High->dP;

    v2 OldPlayerP = Entity.High->P;

    v2 PlayerDelta = 0.5f * ddPlayer * Square(dt) + Entity.High->dP * dt;

    v2 NewPlayerP = OldPlayerP + PlayerDelta;
    Entity.High->dP += ddPlayer * dt;

    for (uint32 Iteration = 0; Iteration < 4; ++Iteration) {

        v2 DesiredPosition = Entity.High->P + PlayerDelta;

        real32 tMin = 1.0f;
        v2 WallNormal = {};
        uint32 HitHighEntityIndex = 0;

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

        Entity.High->P += tMin * PlayerDelta;
        if (HitHighEntityIndex != 0) {
            Entity.High->dP = Entity.High->dP - Inner(Entity.High->dP, WallNormal) * WallNormal;
            PlayerDelta = DesiredPosition - Entity.High->P;
            PlayerDelta = PlayerDelta - Inner(PlayerDelta, WallNormal) * WallNormal;

            high_entity* HitHigh = GameState->HighEntities_ + HitHighEntityIndex;
            low_entity* HitLow = GameState->LowEntities_ + HitHigh->LowEntityIndex;
            HitHigh->AbsTileZ += HitLow->dAbsTileZ;
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

    Entity.Low->P = MapIntoTileSpace(GameState->World, GameState->CameraP, Entity.High->P);
}

internal void SetCamera(game_state* GameState, world_position NewCameraP) {

    world* TileMap = GameState->World;
    world_difference dCameraP = Subtract(TileMap, &NewCameraP, &GameState->CameraP);

    GameState->CameraP = NewCameraP;

    v2 EntityOffsetForFrame = {};
    EntityOffsetForFrame = -dCameraP.dXY;

    uint32 TileSpanX = 17 * 3;
    uint32 TileSpanY = 9 * 3;

    rectangle2 CameraBounds =
        RectCenterDim({ 0, 0 }, TileMap->TileSideInMeters * v2{ (real32)TileSpanX, (real32)TileSpanY });

    OffsetAndCheckFrequencyByArea(GameState, EntityOffsetForFrame, CameraBounds);

    int32 MinTileX = NewCameraP.AbsTileX - TileSpanX / 2;
    int32 MaxTileX = NewCameraP.AbsTileX + TileSpanX / 2;
    int32 MinTileY = NewCameraP.AbsTileY - TileSpanY / 2;
    int32 MaxTileY = NewCameraP.AbsTileY + TileSpanY / 2;

    for (uint32 EntityIndex = 1; EntityIndex < GameState->LowEntityCount; ++EntityIndex) {
        low_entity* Low = GameState->LowEntities_ + EntityIndex;

        if (Low->HighEntityIndex == 0) {
            if (Low->P.AbsTileZ == NewCameraP.AbsTileZ &&
                Low->P.AbsTileX >= MinTileX &&
                Low->P.AbsTileX <= MaxTileX &&
                Low->P.AbsTileY >= MinTileY &&
                Low->P.AbsTileY <= MaxTileY) {
                MakeEntityHigFrequency(GameState, EntityIndex);
            }
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
        AddLowEntity(GameState, EntityType_Null);
        GameState->HighEntityCount = 1;

        GameState->Backdrop =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");
        GameState->HeroShadow =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_shadow.bmp");

        hero_bitmaps* Bitmap;

        Bitmap = &GameState->HeroBitmaps[0];
        Bitmap->Head =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_head.bmp");
        Bitmap->Cape =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_cape.bmp");
        Bitmap->Torso =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;

        ++Bitmap;
        Bitmap->Head =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_head.bmp");
        Bitmap->Cape =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_cape.bmp");
        Bitmap->Torso =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;

        ++Bitmap;
        Bitmap->Head =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_head.bmp");
        Bitmap->Cape =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_cape.bmp");
        Bitmap->Torso =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;

        ++Bitmap;
        Bitmap->Head =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_head.bmp");
        Bitmap->Cape =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_cape.bmp");
        Bitmap->Torso =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;

        InitializeArena(
            &GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
            (uint8*)Memory->PermanentStorage + sizeof(game_state)
        );

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world* TileMap = GameState->World;
        InitializeWorld(TileMap, 1.4f);

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

        for (uint32 ScreenIndex = 0; ScreenIndex < 2; ++ScreenIndex) {

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

        world_position NewCameraP = {};
        NewCameraP.AbsTileX = ScreenBaseX * TilesPerWidth + 17 / 2;
        NewCameraP.AbsTileY = ScreenBaseY * TilesPerHeight + 9 / 2;
        NewCameraP.AbsTileZ = ScreenBaseZ;
        SetCamera(GameState, NewCameraP);

        Memory->IsInitialized = true;
    }

    world* World = GameState->World;
    world* TileMap = World;

    uint32 TileSideInPixels = 60;
    real32 MetersToPixels = (real32)TileSideInPixels / (real32)TileMap->TileSideInMeters;

#define TILES_PER_CHUNK_COUNT 256

    real32 LowerLeftX = -(real32)TileSideInPixels / 2;
    real32 LowerLeftY = (real32)Buffer->Height;

    for (int32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex) {

        game_controller_input* Controller = GetController(Input, ControllerIndex);

        uint32 LowIndex = GameState->PlayerIndexForController[ControllerIndex];
        if (LowIndex == 0) {
            if (Controller->Start.EndedDown) {
                uint32 Index = AddPlayer(GameState);
                GameState->PlayerIndexForController[ControllerIndex] = Index;
            }
        } else {

            entity ControllingEntity = GetHighEntity(GameState, LowIndex);

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
                if (Controller->ActionUp.EndedDown) {
                    ControllingEntity.High->dZ = 3.0f;
                }
            }

            MovePlayer(GameState, ControllingEntity, Input->dtForFrame, ddPlayer);
        }
    }

    entity CameraFollowingEntity = GetHighEntity(GameState, GameState->CameraFollowingEntityIndex);
    if (CameraFollowingEntity.High != 0) {

        world_position NewCameraP = GameState->CameraP;

        NewCameraP.AbsTileZ = CameraFollowingEntity.Low->P.AbsTileZ;

#if 1
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
    DrawRectangle(Buffer, { 0, 0 }, { (real32)Buffer->Width, (real32)Buffer->Height }, 1.0f, 0.0f, 1.0f);

    //* Draw backdrop
    DrawBMP(Buffer, GameState->Backdrop, 0.0f, 0.0f);

    //* Draw world
    real32 ScreenCenterX = (real32)Buffer->Width * 0.5f;
    real32 ScreenCenterY = (real32)Buffer->Height * 0.5f;

#if 0
    for (int32 RelRow = -10; RelRow < 10; ++RelRow) {
        for (int32 RelColumn = -20; RelColumn < 20; ++RelColumn) {

            uint32 Column = RelColumn + GameState->CameraP.AbsTileX;
            uint32 Row = RelRow + GameState->CameraP.AbsTileY;

            uint32 TileId = GetTileValue(TileMap, Column, Row, GameState->CameraP.AbsTileZ);

            if (TileId == 0 || TileId == 1) {
                continue;
            }

            real32 Color = 0.5f;
            if (TileId == 2) {
                Color = 1.0f;
            }

            if (TileId > 2) {
                Color = 0.25f;
            }

            if (Row == GameState->CameraP.AbsTileY && Column == GameState->CameraP.AbsTileX) {
                Color = 0.0f;
            }

            v2 Cen = {
                ScreenCenterX - MetersToPixels * GameState->CameraP.Offset_.X + ((real32)(RelColumn)) * TileSideInPixels,
                ScreenCenterY + MetersToPixels * GameState->CameraP.Offset_.Y - ((real32)(RelRow)) * TileSideInPixels
            };
            v2 TileSide = { (real32)TileSideInPixels, (real32)TileSideInPixels };

            v2 Min = Cen - 0.5f * TileSide;
            v2 Max = Min + TileSide;

            DrawRectangle(Buffer, Min, Max, Color, Color, Color);
        }
    }
#endif

    //* Draw player
    for (uint32 HighEntityIndex = 1; HighEntityIndex < GameState->HighEntityCount; HighEntityIndex++) {
        high_entity* HighEntity = GameState->HighEntities_ + HighEntityIndex;
        low_entity* LowEntity = GameState->LowEntities_ + HighEntity->LowEntityIndex;

        real32 dt = Input->dtForFrame;
        real32 ddZ = -9.8f;
        HighEntity->Z += 0.5f * ddZ * Square(dt) + HighEntity->dZ * dt;
        HighEntity->dZ = ddZ * dt + HighEntity->dZ;
        if (HighEntity->Z < 0) {
            HighEntity->Z = 0;
        }

        real32 PlayerR = 1;
        real32 PlayerG = 1;
        real32 PlayerB = 0;

        real32 PlayerGroundX = ScreenCenterX + HighEntity->P.X * MetersToPixels;
        real32 PlayerGroundY = ScreenCenterY - HighEntity->P.Y * MetersToPixels;

        real32 Z = -HighEntity->Z * MetersToPixels;

        v2 PlayerLeftTop = {
            PlayerGroundX - MetersToPixels * 0.5f * LowEntity->Width,
            PlayerGroundY - 0.5f * MetersToPixels * LowEntity->Height
        };

        v2 PlayerWidthHeight = { LowEntity->Width, LowEntity->Height };
        if (LowEntity->Type == EntityType_Hero) {
            DrawRectangle(
                Buffer,
                PlayerLeftTop,
                PlayerLeftTop + PlayerWidthHeight * MetersToPixels,
                PlayerR, PlayerG, PlayerB
            );
            hero_bitmaps* HeroBitmaps = &GameState->HeroBitmaps[HighEntity->FacingDirection];
            DrawBMP(Buffer, GameState->HeroShadow, PlayerGroundX, PlayerGroundY, HeroBitmaps->AlignX, HeroBitmaps->AlignY, Maximum(0.0f, 1.0f - HighEntity->Z));
            DrawBMP(Buffer, HeroBitmaps->Head, PlayerGroundX, PlayerGroundY + Z, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
            DrawBMP(Buffer, HeroBitmaps->Torso, PlayerGroundX, PlayerGroundY + Z, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
            DrawBMP(Buffer, HeroBitmaps->Cape, PlayerGroundX, PlayerGroundY + Z, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
        } else {
            DrawRectangle(
                Buffer,
                PlayerLeftTop,
                PlayerLeftTop + PlayerWidthHeight * MetersToPixels,
                PlayerR, PlayerG, PlayerB
            );
        }
    }
}
