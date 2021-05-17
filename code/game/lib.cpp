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

internal void InitPlayer(entity* Entity) {
    Entity->Exists = true;

    Entity->P.AbsTileX = 3;
    Entity->P.AbsTileY = 3;
    Entity->P.AbsTileZ = 0;

    Entity->P.Offset.X = 5.0f;
    Entity->P.Offset.Y = 5.5f;

    Entity->Height = 1.4f;
    Entity->Width = 0.75f * Entity->Height;
}

internal entity* GetEntity(game_state* GameState, uint32 Index) {
    entity* Entity = 0;

    if (Index > 0 && Index < ArrayCount(GameState->Entities)) {
        Entity = &GameState->Entities[Index];
    }

    return Entity;
}

internal uint32 AddEntity(game_state* GameState) {
    Assert(GameState->EntityCount < ArrayCount(GameState->Entities));
    uint32 EntityIndex = GameState->EntityCount++;
    entity* Entity = &GameState->Entities[EntityIndex];
    *Entity = {};
    return EntityIndex;
}

internal void MovePlayer(game_state* GameState, entity* Entity, real32 dt, v2 ddPlayer) {

    tile_map* TileMap = GameState->World->TileMap;

    real32 ddPlayerLengthSq = LengthSq(ddPlayer);
    if (ddPlayerLengthSq > 1.0f) {
        ddPlayer *= 1.0f / SquareRoot(ddPlayerLengthSq);
    }

    real32 PlayerAcceleration = 50.0f;

    ddPlayer *= PlayerAcceleration;

    ddPlayer += -8.0f * Entity->dP;

    tile_map_position OldPlayerP = Entity->P;
    tile_map_position NewPlayerP = OldPlayerP;

    v2 PlayerDelta = 0.5f * ddPlayer * Square(dt) + Entity->dP * dt;

    NewPlayerP.Offset += PlayerDelta;
    Entity->dP += ddPlayer * dt;
    NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);

#if 1
    tile_map_position PlayerLeft = NewPlayerP;
    PlayerLeft.Offset.X -= 0.5f * Entity->Width;
    PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);

    tile_map_position PlayerRight = NewPlayerP;
    PlayerRight.Offset.X += 0.5f * Entity->Width;
    PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);

    bool32 Collided = false;
    tile_map_position ColP = {};
    if (!IsTileMapPointEmpty(TileMap, NewPlayerP)) {
        ColP = NewPlayerP;
        Collided = true;
    }
    if (!IsTileMapPointEmpty(TileMap, PlayerLeft)) {
        ColP = PlayerLeft;
        Collided = true;
    }
    if (!IsTileMapPointEmpty(TileMap, PlayerRight)) {
        ColP = PlayerRight;
        Collided = true;
    }

    if (!Collided) {
        Entity->P = NewPlayerP;
    } else {
        v2 r = {};
        if (ColP.AbsTileX < Entity->P.AbsTileX) {
            r = { 1, 0 };
        }
        if (ColP.AbsTileX > Entity->P.AbsTileX) {
            r = { -1, 0 };
        }
        if (ColP.AbsTileY < Entity->P.AbsTileY) {
            r = { 0, 1 };
        }
        if (ColP.AbsTileY > Entity->P.AbsTileY) {
            r = { 0, -1 };
        }
        Entity->dP = Entity->dP - 1 * Inner(Entity->dP, r) * r;
    }
#else
    uint32 MinTileX = Minimum(OldPlayerP.AbsTileX, NewPlayerP.AbsTileX);
    uint32 MinTileY = Minimum(OldPlayerP.AbsTileY, NewPlayerP.AbsTileY);
    uint32 OnePastMaxTileX = Maximum(OldPlayerP.AbsTileX, NewPlayerP.AbsTileX) + 1;
    uint32 OnePastMaxTileY = Maximum(OldPlayerP.AbsTileY, NewPlayerP.AbsTileY) + 1;

    uint32 AbsTileZ = Entity->P.AbsTileZ;
    real32 tMin = 1.0f;

    for (uint32 AbsTileY = MinTileY; AbsTileY != OnePastMaxTileY; AbsTileY++) {
        for (uint32 AbsTileX = MinTileX; AbsTileX != OnePastMaxTileX; AbsTileX++) {
            tile_map_position TestTileP = CenteredTilePoint(AbsTileX, AbsTileY, AbsTileZ);
            uint32 TileValue = GetTileValue(TileMap, AbsTileX, AbsTileY, AbsTileZ);
            if (IsTileValueEmpty(TileValue)) {
                v2 MinCorner = -0.5f * v2{ TileMap->TileSideInMeters, TileMap->TileSideInMeters };
                v2 MaxCorner = 0.5f * v2{ TileMap->TileSideInMeters, TileMap->TileSideInMeters };

                tile_map_difference RelNewPlayerP = Subtract(TileMap, &TestTileP, &NewPlayerP);
                v2 Rel = RelNewPlayerP.dXY;

                tResult = (WallX - Rel.X) / PlayerDelta.X;

                TestWall(MinCorner.X, MinCorner.Y, MaxCorner.X, MaxCorner.Y, Rel.X, Rel.Y);
            }
        }
    }
#endif
    if (!AreOnSameTile(&OldPlayerP, &Entity->P)) {
        uint32 NewTileValue = GetTileValue(TileMap, Entity->P);
        if (NewTileValue == 3) {
            Entity->P.AbsTileZ++;
        } else if (NewTileValue == 4) {
            Entity->P.AbsTileZ--;
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
        entity* ControllingEntity = GetEntity(GameState, GameState->PlayerIndexForController[ControllerIndex]);

        if (ControllingEntity == 0) {
            if (Controller->Start.EndedDown) {
                uint32 Index = AddEntity(GameState);
                GameState->PlayerIndexForController[ControllerIndex] = Index;
                ControllingEntity = GetEntity(GameState, Index);
                InitPlayer(ControllingEntity);
                if (GameState->CameraFollowingEntityIndex == 0) {
                    GameState->CameraFollowingEntityIndex = Index;
                }
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
        }

        MovePlayer(GameState, ControllingEntity, Input->dtForFrame, ddPlayer);
    }

    entity* CameraFollowingEntity = GetEntity(GameState, GameState->CameraFollowingEntityIndex);
    if (CameraFollowingEntity) {
        GameState->CameraP.AbsTileZ = CameraFollowingEntity->P.AbsTileZ;
        tile_map_difference Diff = Subtract(GameState->World->TileMap, &CameraFollowingEntity->P, &GameState->CameraP);
        if (Diff.dXY.X > 9.0f * TileMap->TileSideInMeters) {
            GameState->CameraP.AbsTileX += 17;
        } else if (Diff.dXY.X < -9.0f * TileMap->TileSideInMeters) {
            GameState->CameraP.AbsTileX -= 17;
        }
        if (Diff.dXY.Y > 5.0f * TileMap->TileSideInMeters) {
            GameState->CameraP.AbsTileY += 9;
        } else if (Diff.dXY.Y < -5.0f * TileMap->TileSideInMeters) {
            GameState->CameraP.AbsTileY -= 9;
        }
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
                ScreenCenterX - MetersToPixels * GameState->CameraP.Offset.X + ((real32)(RelColumn)) * TileSideInPixels,
                ScreenCenterY + MetersToPixels * GameState->CameraP.Offset.Y - ((real32)(RelRow)) * TileSideInPixels
            };
            v2 TileSide = { (real32)TileSideInPixels, (real32)TileSideInPixels };

            v2 Min = Cen - 0.5f * TileSide;
            v2 Max = Min + TileSide;

            DrawRectangle(Buffer, Min, Max, Color, Color, Color);
        }
    }

    //* Draw player
    entity* Entity = GameState->Entities;
    for (uint32 EntityIndex = 0; EntityIndex < GameState->EntityCount; EntityIndex++, Entity++) {

        if (!Entity->Exists) {
            continue;
        }

        tile_map_difference Diff = Subtract(GameState->World->TileMap, &Entity->P, &GameState->CameraP);

        real32 PlayerR = 1;
        real32 PlayerG = 1;
        real32 PlayerB = 0;

        real32 PlayerGroundX = ScreenCenterX + Diff.dXY.X * MetersToPixels;
        real32 PlayerGroundY = ScreenCenterY - Diff.dXY.Y * MetersToPixels;

        v2 PlayerLeftTop = { PlayerGroundX - MetersToPixels * 0.5f * Entity->Width, PlayerGroundY - MetersToPixels * Entity->Height };

        v2 PlayerWidthHeight = { Entity->Width, Entity->Height };
        DrawRectangle(
            Buffer,
            PlayerLeftTop,
            PlayerLeftTop + PlayerWidthHeight * MetersToPixels,
            PlayerR, PlayerG, PlayerB
        );
        hero_bitmaps* HeroBitmaps = &GameState->HeroBitmaps[Entity->FacingDirection];
        DrawBMP(Buffer, HeroBitmaps->Head, PlayerGroundX, PlayerGroundY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
        DrawBMP(Buffer, HeroBitmaps->Torso, PlayerGroundX, PlayerGroundY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
        DrawBMP(Buffer, HeroBitmaps->Cape, PlayerGroundX, PlayerGroundY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
    }
}
