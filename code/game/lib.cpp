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

        GameState->PlayerP.AbsTileX = 3;
        GameState->PlayerP.AbsTileY = 3;
        GameState->PlayerP.AbsTileZ = 0;

        GameState->PlayerP.Offset.X = 5.0f;
        GameState->PlayerP.Offset.Y = 5.5f;

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

    real32 PlayerHeight = 1.4f;
    real32 PlayerWidth = 0.75f * PlayerHeight;

    tile_map_position OldPlayerP = GameState->PlayerP;

    for (int32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex) {
        game_controller_input* Controller = &(Input->Controllers[ControllerIndex]);

        real32 PlayerAcceleration = 10.0f;
        if (Controller->ActionUp.EndedDown) {
            PlayerAcceleration = 50.0f;
        }

        v2 ddPlayer = {};

        if (Controller->MoveUp.EndedDown) {
            GameState->HeroFacingDirection = 1;
            ddPlayer.Y = 1;
        }
        if (Controller->MoveDown.EndedDown) {
            GameState->HeroFacingDirection = 3;
            ddPlayer.Y = -1;
        }
        if (Controller->MoveLeft.EndedDown) {
            GameState->HeroFacingDirection = 2;
            ddPlayer.X = -1;
        }
        if (Controller->MoveRight.EndedDown) {
            GameState->HeroFacingDirection = 0;
            ddPlayer.X = 1;
        }

        if (ddPlayer.X != 0 && ddPlayer.Y != 0) {
            ddPlayer *= 0.707106f;
        }

        ddPlayer *= PlayerAcceleration;

        ddPlayer += -1.5f * GameState->dPlayerP;

        tile_map_position NewPlayerP = GameState->PlayerP;

        v2 PlayerDelta = 0.5f * ddPlayer * Square(Input->dtForFrame) +
            GameState->dPlayerP * Input->dtForFrame;

        NewPlayerP.Offset += PlayerDelta;
        GameState->dPlayerP += ddPlayer * Input->dtForFrame;
        NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);

#if 1
        tile_map_position PlayerLeft = NewPlayerP;
        PlayerLeft.Offset.X -= 0.5f * PlayerWidth;
        PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);

        tile_map_position PlayerRight = NewPlayerP;
        PlayerRight.Offset.X += 0.5f * PlayerWidth;
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
            GameState->PlayerP = NewPlayerP;
        } else {
            v2 r = {};
            if (ColP.AbsTileX < GameState->PlayerP.AbsTileX) {
                r = { 1, 0 };
            }
            if (ColP.AbsTileX > GameState->PlayerP.AbsTileX) {
                r = { -1, 0 };
            }
            if (ColP.AbsTileY < GameState->PlayerP.AbsTileY) {
                r = { 0, 1 };
            }
            if (ColP.AbsTileY > GameState->PlayerP.AbsTileY) {
                r = { 0, -1 };
            }
            GameState->dPlayerP = GameState->dPlayerP - 1 * Inner(GameState->dPlayerP, r) * r;
        }
#else
        uint32 MinTileX = 0;
        uint32 MinTileY = 0;
        uint32 OnePastMaxTileX = 0;
        uint32 OnePastMaxTileY = 0;
        uint32 AbsTileZ = GameState->PlayerP.AbsTileZ;
        tile_map_position BestPlayerP = GameState->PlayerP;
        real32 BestDistanceSq = LengthSq(PlayerDelta);
        for (uint32 AbsTileY = MinTileY; AbsTileY != OnePastMaxTileY; AbsTileY++) {
            for (uint32 AbsTileX = MinTileX; AbsTileX != OnePastMaxTileX; AbsTileX++) {
                tile_map_position TestTileP = CenteredTilePoint(AbsTileX, AbsTileY, AbsTileZ);
                uint32 TileValue = GetTileValue(TileMap, AbsTileX, AbsTileY, AbsTileZ);
                if (IsTileValueEmpty(TileValue)) {
                    v2 MinCorner = -0.5f * v2{ TileMap->TileSideInMeters, TileMap->TileSideInMeters };
                    v2 MaxCorner = 0.5f * v2{ TileMap->TileSideInMeters, TileMap->TileSideInMeters };
                    tile_map_difference RelNewPlayerP = Subtract(TileMap, &TestTileP, &NewPlayerP);
                    v2 TestP = ClosestPointInRectangle(MinCorner, MaxCorner, RelNewPlayerP);
                    TestDistanceSq = ;
                    if (BestDistanceSq > TestDistanceSq) {
                        BestPlayerP = TestP;
                        BestDistanceSq = TestDistanceSq;
                    }
                }
            }
        }
#endif
    }

    if (!AreOnSameTile(&OldPlayerP, &GameState->PlayerP)) {
        uint32 NewTileValue = GetTileValue(TileMap, GameState->PlayerP);
        if (NewTileValue == 3) {
            GameState->PlayerP.AbsTileZ++;
        } else if (NewTileValue == 4) {
            GameState->PlayerP.AbsTileZ--;
        }
    }
    GameState->CameraP.AbsTileZ = GameState->PlayerP.AbsTileZ;

    tile_map_difference Diff = Subtract(GameState->World->TileMap, &GameState->PlayerP, &GameState->CameraP);
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
    Diff = Subtract(GameState->World->TileMap, &GameState->PlayerP, &GameState->CameraP);

    real32 PlayerR = 1;
    real32 PlayerG = 1;
    real32 PlayerB = 0;

    real32 PlayerGroundX = ScreenCenterX + Diff.dXY.X * MetersToPixels;
    real32 PlayerGroundY = ScreenCenterY - Diff.dXY.Y * MetersToPixels;

    v2 PlayerLeftTop = { PlayerGroundX - MetersToPixels * 0.5f * PlayerWidth, PlayerGroundY - MetersToPixels * PlayerHeight };

    v2 PlayerWidthHeight = { PlayerWidth, PlayerHeight };
    DrawRectangle(
        Buffer,
        PlayerLeftTop,
        PlayerLeftTop + PlayerWidthHeight * MetersToPixels,
        PlayerR, PlayerG, PlayerB
    );
    hero_bitmaps* HeroBitmaps = &GameState->HeroBitmaps[GameState->HeroFacingDirection];
    DrawBMP(Buffer, HeroBitmaps->Head, PlayerGroundX, PlayerGroundY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
    DrawBMP(Buffer, HeroBitmaps->Torso, PlayerGroundX, PlayerGroundY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
    DrawBMP(Buffer, HeroBitmaps->Cape, PlayerGroundX, PlayerGroundY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
}
