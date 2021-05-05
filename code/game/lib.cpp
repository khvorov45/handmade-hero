#include "lib.hpp"
#include "tile.cpp"
#include "world.cpp"
#include "state.cpp"
#include "random.cpp"
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
    real32 RealMinX, real32 RealMinY, real32 RealMaxX, real32 RealMaxY,
    real32 R, real32 G, real32 B
) {
    int32 MinX = RoundReal32ToInt32(RealMinX);
    int32 MinY = RoundReal32ToInt32(RealMinY);
    int32 MaxX = RoundReal32ToInt32(RealMaxX);
    int32 MaxY = RoundReal32ToInt32(RealMaxY);

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
        GameState->PlayerP.AbsTileX = 3;
        GameState->PlayerP.AbsTileY = 3;
        GameState->PlayerP.XTileRel = 5.0f;
        GameState->PlayerP.YTileRel = 5.5f;
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

        TileMap->TileChunks = PushArray(
            &GameState->WorldArena, TileMap->TileChunkCountX * TileMap->TileChunkCountY, tile_chunk
        );

        for (uint32 TileChunkY = 0; TileChunkY < TileMap->TileChunkCountY; ++TileChunkY) {
            for (uint32 TileChunkX = 0; TileChunkX < TileMap->TileChunkCountX; ++TileChunkX) {
                TileMap->TileChunks[TileChunkY * TileMap->TileChunkCountX + TileChunkX].Tiles =
                    PushArray(&GameState->WorldArena, TileMap->ChunkDim * TileMap->ChunkDim, uint32);
            }
        }

        TileMap->TileSideInMeters = 1.4f;
        TileMap->TileSideInPixels = 6;

        TileMap->MetersToPixels = (real32)TileMap->TileSideInPixels / (real32)TileMap->TileSideInMeters;

        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;
        uint32 ScreenX = 0;
        uint32 ScreenY = 0;
        uint32 RandomNumberIndex = 0;
        for (uint32 ScreenIndex = 0; ScreenIndex < 100; ++ScreenIndex) {
            for (uint32 TileY = 0; TileY < TilesPerHeight; ++TileY) {
                for (uint32 TileX = 0; TileX < TilesPerWidth; ++TileX) {
                    uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;

                    uint32 TileValue = 1;
                    if (TileX == 0 || TileX == TilesPerWidth - 1) {
                        if (TileY != TilesPerHeight / 2) {
                            TileValue = 2;
                        }
                    }
                    if (TileY == 0 || TileY == TilesPerHeight - 1) {
                        if (TileX != TilesPerWidth / 2) {
                            TileValue = 2;
                        }
                    }

                    SetTileValue(
                        &GameState->WorldArena, World->TileMap, AbsTileX, AbsTileY,
                        TileValue
                    );
                }
            }
            Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
            uint32 RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
            if (RandomChoice == 0) {
                ScreenX += 1;
            } else {
                ScreenY += 1;
            }
        }

        Memory->IsInitialized = true;
    }

    world* World = GameState->World;
    tile_map* TileMap = World->TileMap;

#define TILES_PER_CHUNK_COUNT 256

    real32 LowerLeftX = -(real32)TileMap->TileSideInPixels / 2;
    real32 LowerLeftY = (real32)Buffer->Height;

    real32 PlayerHeight = 1.4f;
    real32 PlayerWidth = 0.75f * PlayerHeight;

    for (int32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex) {
        game_controller_input* Controller = &(Input->Controllers[ControllerIndex]);

        real32 PlayerSpeed = 3.0f;
        if (Controller->ActionUp.EndedDown) {
            PlayerSpeed = 10.0f;
        }

        real32 dPlayerX = 0;
        real32 dPlayerY = 0;

        if (Controller->MoveUp.EndedDown) {
            dPlayerY = 1;
        }
        if (Controller->MoveDown.EndedDown) {
            dPlayerY = -1;
        }
        if (Controller->MoveLeft.EndedDown) {
            dPlayerX = -1;
        }
        if (Controller->MoveRight.EndedDown) {
            dPlayerX = 1;
        }

        tile_map_position NewPlayerP = GameState->PlayerP;

        NewPlayerP.XTileRel += dPlayerX * PlayerSpeed * Input->dtForFrame;;
        NewPlayerP.YTileRel += dPlayerY * PlayerSpeed * Input->dtForFrame;;
        NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);

        tile_map_position PlayerLeft = NewPlayerP;
        PlayerLeft.XTileRel -= 0.5f * PlayerWidth;
        PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);

        tile_map_position PlayerRight = NewPlayerP;
        PlayerRight.XTileRel += 0.5f * PlayerWidth;
        PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);

        if (IsTileMapPointEmpty(TileMap, NewPlayerP) &&
            IsTileMapPointEmpty(TileMap, PlayerLeft) &&
            IsTileMapPointEmpty(TileMap, PlayerRight)) {
            GameState->PlayerP = NewPlayerP;
        }
    }

    DrawRectangle(Buffer, 0, 0, (real32)Buffer->Width, (real32)Buffer->Height, 1.0f, 0.0f, 1.0f);

    real32 ScreenCenterX = (real32)Buffer->Width * 0.5f;
    real32 ScreenCenterY = (real32)Buffer->Height * 0.5f;

    for (int32 RelRow = -100; RelRow < 100; ++RelRow) {
        for (int32 RelColumn = -200; RelColumn < 200; ++RelColumn) {

            uint32 Column = RelColumn + GameState->PlayerP.AbsTileX;
            uint32 Row = RelRow + GameState->PlayerP.AbsTileY;

            uint32 TileId = GetTileValue(TileMap, Column, Row);

            if (TileId == 0) {
                continue;
            }

            real32 Color = 0.5f;
            if (TileId == 2) {
                Color = 1.0f;
            }

            if (Row == GameState->PlayerP.AbsTileY && Column == GameState->PlayerP.AbsTileX) {
                Color = 0.0f;
            }

            real32 CenX = ScreenCenterX - TileMap->MetersToPixels * GameState->PlayerP.XTileRel + ((real32)(RelColumn)) * TileMap->TileSideInPixels;
            real32 CenY = ScreenCenterY + TileMap->MetersToPixels * GameState->PlayerP.YTileRel - ((real32)(RelRow)) * TileMap->TileSideInPixels;

            real32 MinX = CenX - 0.5f * TileMap->TileSideInPixels;
            real32 MinY = CenY - 0.5f * TileMap->TileSideInPixels;

            real32 MaxX = MinX + TileMap->TileSideInPixels;
            real32 MaxY = MinY + TileMap->TileSideInPixels;

            DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Color, Color, Color);
        }
    }

    real32 PlayerR = 1;
    real32 PlayerG = 1;
    real32 PlayerB = 0;

    real32 PlayerMinX = ScreenCenterX - TileMap->MetersToPixels * 0.5f * PlayerWidth;
    real32 PlayerMinY = ScreenCenterY - TileMap->MetersToPixels * PlayerHeight;
    DrawRectangle(
        Buffer,
        PlayerMinX, PlayerMinY,
        PlayerMinX + TileMap->MetersToPixels * PlayerWidth, PlayerMinY + TileMap->MetersToPixels * PlayerHeight,
        PlayerR, PlayerG, PlayerB
    );
}
