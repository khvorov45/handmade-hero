#include "lib.hpp"
#include "tile.cpp"
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
    int32 AlignX = 0, int32 AlignY = 0
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

            real32 A = (real32)((*Source >> 24)) / 255.0f;

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

internal void ChangeEntityResidence(game_state* GameState, uint32 EntityIndex, entity_residence Residence) {
    if (Residence == EntityResidence_High && GameState->EntityResidence[EntityIndex] != EntityResidence_High) {
        high_entity* EntityHigh = &GameState->HighEntities[EntityIndex];
        dormant_entity* EntityDormant = &GameState->DormantEntities[EntityIndex];

        tile_map_difference Diff = Subtract(GameState->World->TileMap, &EntityDormant->P, &GameState->CameraP);
        EntityHigh->P = Diff.dXY;
        EntityHigh->dP = { 0.0f, 0.0f };
        EntityHigh->AbsTileZ = EntityDormant->P.AbsTileZ;
        EntityHigh->FacingDirection = 0;
        GameState->EntityResidence[EntityIndex] = Residence;
    }
}

internal entity GetEntity(game_state* GameState, entity_residence Residence, uint32 Index) {
    entity Entity = {};

    if (Index > 0 && Index < GameState->EntityCount) {
        if (GameState->EntityResidence[Index] < Residence) {
            ChangeEntityResidence(GameState, Index, Residence);
            Assert(GameState->EntityResidence[Index] >= Residence);
        }

        Entity.Residence = Residence;
        Entity.Dormant = &GameState->DormantEntities[Index];
        Entity.Low = &GameState->LowEntities[Index];
        Entity.High = &GameState->HighEntities[Index];
    }

    return Entity;
}

internal void InitPlayer(game_state* GameState, uint32 EntityIndex) {
    entity Entity = GetEntity(GameState, EntityResidence_Dormant, EntityIndex);

    Entity.Dormant->P.AbsTileX = 1;
    Entity.Dormant->P.AbsTileY = 3;
    Entity.Dormant->P.AbsTileZ = 0;

    Entity.Dormant->P.Offset_.X = 0.0f;
    Entity.Dormant->P.Offset_.Y = 0.0f;

    Entity.Dormant->Height = 0.5f;
    Entity.Dormant->Width = 1.0f;

    Entity.Dormant->Collides = true;

    ChangeEntityResidence(GameState, EntityIndex, EntityResidence_High);

    if (GetEntity(GameState, EntityResidence_Dormant, GameState->CameraFollowingEntityIndex).Residence == EntityResidence_Nonexistant) {
        GameState->CameraFollowingEntityIndex = EntityIndex;
    }
}

internal uint32 AddEntity(game_state* GameState) {
    uint32 EntityIndex = GameState->EntityCount++;

    Assert(GameState->EntityCount < ArrayCount(GameState->DormantEntities));
    Assert(GameState->EntityCount < ArrayCount(GameState->LowEntities));
    Assert(GameState->EntityCount < ArrayCount(GameState->HighEntities));

    GameState->EntityResidence[EntityIndex] = EntityResidence_Dormant;
    GameState->DormantEntities[EntityIndex] = {};
    GameState->LowEntities[EntityIndex] = {};
    GameState->HighEntities[EntityIndex] = {};

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

    tile_map* TileMap = GameState->World->TileMap;

    real32 ddPlayerLengthSq = LengthSq(ddPlayer);
    if (ddPlayerLengthSq > 1.0f) {
        ddPlayer *= 1.0f / SquareRoot(ddPlayerLengthSq);
    }

    real32 PlayerAcceleration = 50.0f;

    ddPlayer *= PlayerAcceleration;

    ddPlayer += -8.0f * Entity.High->dP;

    v2 OldPlayerP = Entity.High->P;

    v2 PlayerDelta = 0.5f * ddPlayer * Square(dt) + Entity.High->dP * dt;

    v2 NewPlayerP = OldPlayerP + PlayerDelta;
    Entity.High->dP += ddPlayer * dt;


    /*

    uint32 MinTileX = Minimum(OldPlayerP.AbsTileX, NewPlayerP.AbsTileX);
    uint32 MinTileY = Minimum(OldPlayerP.AbsTileY, NewPlayerP.AbsTileY);
    uint32 MaxTileX = Maximum(OldPlayerP.AbsTileX, NewPlayerP.AbsTileX);
    uint32 MaxTileY = Maximum(OldPlayerP.AbsTileY, NewPlayerP.AbsTileY);

    uint32 EntityTileWidth = CeilReal32ToUint32(Entity.High->Width / TileMap->TileSideInMeters);
    uint32 EntityTileHeight = CeilReal32ToUint32(Entity.High->Height / TileMap->TileSideInMeters);

    MinTileX -= EntityTileWidth;
    MinTileY -= EntityTileHeight;
    MaxTileX += EntityTileWidth;
    MaxTileY += EntityTileHeight;

    uint32 AbsTileZ = Entity.High->P.AbsTileZ;
    */

    real32 tRemaining = 1.0f;
    for (uint32 Iteration = 0; Iteration < 4 && tRemaining > 0.0f; ++Iteration) {
        real32 tMin = 1.0f;
        v2 WallNormal = {};
        uint32 HitEntityIndex = 0;

        for (uint32 EntityIndex = 1; EntityIndex < GameState->EntityCount; ++EntityIndex) {

            entity TestEntity = GetEntity(GameState, EntityResidence_High, EntityIndex);

            if (!TestEntity.Dormant->Collides || TestEntity.High == Entity.High) {
                continue;
            }

            real32 DiameterW = Entity.Dormant->Width + TestEntity.Dormant->Width;
            real32 DiameterH = Entity.Dormant->Height + TestEntity.Dormant->Height;

            v2 MinCorner = -0.5f * v2{ DiameterW, DiameterH };
            v2 MaxCorner = 0.5f * v2{ DiameterW, DiameterH };

            v2 Rel = Entity.High->P - TestEntity.High->P;

            if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y)) {
                WallNormal = { -1, 0 };
                HitEntityIndex = EntityIndex;
            }
            if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y)) {
                WallNormal = { 1, 0 };
                HitEntityIndex = EntityIndex;
            }
            if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X)) {
                WallNormal = { 0, -1 };
                HitEntityIndex = EntityIndex;
            }
            if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X)) {
                WallNormal = { 0, 1 };
                HitEntityIndex = EntityIndex;
            }
        }

        Entity.High->P += tMin * PlayerDelta;
        if (HitEntityIndex != 0) {
            Entity.High->dP = Entity.High->dP - Inner(Entity.High->dP, WallNormal) * WallNormal;
            PlayerDelta = PlayerDelta - Inner(PlayerDelta, WallNormal) * WallNormal;
            tRemaining -= tMin * tRemaining;

            entity HitEntity = GetEntity(GameState, EntityResidence_Dormant, HitEntityIndex);
            Entity.High->AbsTileZ += HitEntity.Dormant->dAbsTileZ;
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

    Entity.Dormant->P = MapIntoTileSpace(GameState->World->TileMap, GameState->CameraP, Entity.High->P);
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
        AddEntity(GameState); //* Null entity

        GameState->Backdrop =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");

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

        GameState->CameraP.AbsTileX = 17 / 2;
        GameState->CameraP.AbsTileY = 9 / 2;
        GameState->CameraP.AbsTileZ = 0;

        InitializeArena(
            &GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
            (uint8*)Memory->PermanentStorage + sizeof(game_state)
        );

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world* World = GameState->World;
        World->TileMap = PushStruct(&GameState->WorldArena, tile_map);
        tile_map* TileMap = World->TileMap;

        TileMap->ChunkShift = 4;
        TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
        TileMap->ChunkDim = (1 << TileMap->ChunkShift);

        TileMap->TileChunkCountX = 128;
        TileMap->TileChunkCountY = 128;
        TileMap->TileChunkCountZ = 2;

        TileMap->TileChunks = PushArray(
            &GameState->WorldArena,
            TileMap->TileChunkCountX * TileMap->TileChunkCountY * TileMap->TileChunkCountZ,
            tile_chunk
        );

        TileMap->TileSideInMeters = 1.4f;

        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;
        uint32 ScreenX = 0;
        uint32 ScreenY = 0;
        uint32 AbsTileZ = 0;
        uint32 RandomNumberIndex = 0;

        bool32 DoorLeft = false;
        bool32 DoorRight = false;
        bool32 DoorTop = false;
        bool32 DoorBottom = false;
        bool32 DoorUp = false;
        bool32 DoorDown = false;

        for (uint32 ScreenIndex = 0; ScreenIndex < 100; ++ScreenIndex) {

            Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));

            uint32 RandomChoice;
            if (DoorUp || DoorDown) {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
            } else {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
            }

            bool32 CreatedZDoor = false;

            if (RandomChoice == 2) {
                CreatedZDoor = true;
                if (AbsTileZ == 0) {
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

                    SetTileValue(
                        &GameState->WorldArena, World->TileMap, AbsTileX, AbsTileY, AbsTileZ,
                        TileValue
                    );
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
                if (AbsTileZ == 0) {
                    AbsTileZ = 1;
                } else {
                    AbsTileZ = 0;
                }
            } else if (RandomChoice == 1) {
                ScreenX += 1;
            } else {
                ScreenY += 1;
            }

        }

        Memory->IsInitialized = true;
    }

    world* World = GameState->World;
    tile_map* TileMap = World->TileMap;

    uint32 TileSideInPixels = 60;
    real32 MetersToPixels = (real32)TileSideInPixels / (real32)TileMap->TileSideInMeters;

#define TILES_PER_CHUNK_COUNT 256

    real32 LowerLeftX = -(real32)TileSideInPixels / 2;
    real32 LowerLeftY = (real32)Buffer->Height;

    for (int32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex) {

        game_controller_input* Controller = GetController(Input, ControllerIndex);
        entity ControllingEntity = GetEntity(GameState, EntityResidence_High, GameState->PlayerIndexForController[ControllerIndex]);

        if (ControllingEntity.Residence == EntityResidence_Nonexistant) {
            if (Controller->Start.EndedDown) {
                uint32 Index = AddEntity(GameState);
                InitPlayer(GameState, Index);
                GameState->PlayerIndexForController[ControllerIndex] = Index;
            }
            continue;
        }

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

    v2 EntityOffsetForFrame = {};
    entity CameraFollowingEntity = GetEntity(GameState, EntityResidence_High, GameState->CameraFollowingEntityIndex);
    if (CameraFollowingEntity.Residence != EntityResidence_Nonexistant) {

        tile_map_position OldCameraP = GameState->CameraP;

        GameState->CameraP.AbsTileZ = CameraFollowingEntity.Dormant->P.AbsTileZ;

        if (CameraFollowingEntity.High->P.X > 9.0f * TileMap->TileSideInMeters) {
            GameState->CameraP.AbsTileX += 17;
        } else if (CameraFollowingEntity.High->P.X < -9.0f * TileMap->TileSideInMeters) {
            GameState->CameraP.AbsTileX -= 17;
        }
        if (CameraFollowingEntity.High->P.Y > 5.0f * TileMap->TileSideInMeters) {
            GameState->CameraP.AbsTileY += 9;
        } else if (CameraFollowingEntity.High->P.Y < -5.0f * TileMap->TileSideInMeters) {
            GameState->CameraP.AbsTileY -= 9;
        }

        tile_map_difference dCameraP = Subtract(TileMap, &GameState->CameraP, &OldCameraP);
        EntityOffsetForFrame = -dCameraP.dXY;

    }

    //* Clear screen
    DrawRectangle(Buffer, { 0, 0 }, { (real32)Buffer->Width, (real32)Buffer->Height }, 1.0f, 0.0f, 1.0f);

    //* Draw backdrop
    DrawBMP(Buffer, GameState->Backdrop, 0.0f, 0.0f);

    //* Draw world
    real32 ScreenCenterX = (real32)Buffer->Width * 0.5f;
    real32 ScreenCenterY = (real32)Buffer->Height * 0.5f;

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

    //* Draw player
    for (uint32 EntityIndex = 0; EntityIndex < GameState->EntityCount; EntityIndex++) {
        if (GameState->EntityResidence[EntityIndex] == EntityResidence_High) {
            high_entity* HighEntity = &GameState->HighEntities[EntityIndex];
            low_entity* LowEntity = &GameState->LowEntities[EntityIndex];
            dormant_entity* DormantEntity = &GameState->DormantEntities[EntityIndex];

            HighEntity->P += EntityOffsetForFrame;

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
                PlayerGroundX - MetersToPixels * 0.5f * DormantEntity->Width,
                PlayerGroundY - 0.5f * MetersToPixels * DormantEntity->Height
            };

            v2 PlayerWidthHeight = { DormantEntity->Width, DormantEntity->Height };
            DrawRectangle(
                Buffer,
                PlayerLeftTop,
                PlayerLeftTop + PlayerWidthHeight * MetersToPixels,
                PlayerR, PlayerG, PlayerB
            );
            hero_bitmaps* HeroBitmaps = &GameState->HeroBitmaps[HighEntity->FacingDirection];
            DrawBMP(Buffer, HeroBitmaps->Head, PlayerGroundX, PlayerGroundY + Z, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
            DrawBMP(Buffer, HeroBitmaps->Torso, PlayerGroundX, PlayerGroundY + Z, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
            DrawBMP(Buffer, HeroBitmaps->Cape, PlayerGroundX, PlayerGroundY + Z, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
        }
    }
}
