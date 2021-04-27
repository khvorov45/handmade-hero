#include "lib.hpp"
#include <math.h>

internal void GameOutputSound(game_sound_buffer* SoundBuffer) {
    int16* Samples = SoundBuffer->Samples;
    for (int32 SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex) {
        int16 SampleValue = 0;
        *Samples++ = SampleValue;
        *Samples++ = SampleValue;

    }
}

internal inline int32 RoundReal32ToInt32(real32 X) {
    return (int32)(X + 0.5f);
}

internal inline uint32 RoundReal32ToUint32(real32 X) {
    return (uint32)(X + 0.5f);
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

internal inline int32 TruncateReal32ToInt32(real32 Real32) {
    return (int32)Real32;
}

struct tile_map {
    int32 CountX;
    int32 CountY;

    real32 UpperLeftX;
    real32 UpperLeftY;
    real32 TileWidth;
    real32 TileHeight;

    uint32* Tiles;
};

struct world {
    int32 CountX;
    int32 CountY;

    tile_map* TileMaps;
};

internal inline uint32 GetTileValueUnchecked(tile_map* TileMap, int32 TileX, int32 TileY) {
    return TileMap->Tiles[TileY * TileMap->CountX + TileX];
}

internal inline tile_map* GetTilemap(world* World, int32 TileMapX, int32 TileMapY) {
    tile_map* TileMap = 0;
    if ((TileMapX >= 0) && (TileMapX < World->CountX) && (TileMapY >= 0) && (TileMapY < World->CountY)) {
        TileMap = &World->TileMaps[TileMapY * World->CountX + TileMapX];
    }
    return TileMap;
}

internal bool32 IsTileMapPointEmpty(
    tile_map* TileMap,
    real32 TestX, real32 TestY
) {
    int32 NewPlayerTileX = TruncateReal32ToInt32((TestX - TileMap->UpperLeftX) / TileMap->TileWidth);
    int32 NewPlayerTileY = TruncateReal32ToInt32((TestY - TileMap->UpperLeftY) / TileMap->TileHeight);

    bool32 IsEmpty = false;
    if ((NewPlayerTileX >= 0) && (NewPlayerTileX < TileMap->CountX) &&
        (NewPlayerTileY >= 0) && (NewPlayerTileY < TileMap->CountY)) {
        uint32 TileMapValue = GetTileValueUnchecked(TileMap, NewPlayerTileX, NewPlayerTileY);
        IsEmpty = TileMapValue == 0;
    }
    return IsEmpty;
}

internal bool32 IsWorldPointEmpty(
    world* World,
    int32 TileMapX, int32 TileMapY,
    real32 TestX, real32 TestY
) {
    bool32 IsEmpty = false;

    tile_map* TileMap = GetTilemap(World, TileMapX, TileMapY);

    if (TileMap == 0) {
        return IsEmpty;
    }

    return IsTileMapPointEmpty(TileMap, TestX, TestY);
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
        GameState->PlayerX = 100;
        GameState->PlayerY = 100;
        Memory->IsInitialized = true;
    }

#define WORLD_COUNT_X 2
#define WORLD_COUNT_Y 2

#define TILE_MAP_COUNT_X 17
#define TILE_MAP_COUNT_Y 9

    tile_map TileMaps[WORLD_COUNT_Y][WORLD_COUNT_X];
    TileMaps[0][0].CountX = TILE_MAP_COUNT_X;
    TileMaps[0][0].CountY = TILE_MAP_COUNT_Y;
    TileMaps[0][0].UpperLeftX = -30;
    TileMaps[0][0].UpperLeftY = 0;
    TileMaps[0][0].TileWidth = 60;
    TileMaps[0][0].TileHeight = 60;

    uint32 Tiles00[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
           {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 1,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  0},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1},
           {1, 0, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1}
    };
    uint32 Tiles01[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
           {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1}
    };
    uint32 Tiles10[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
           {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1}
    };
    uint32 Tiles11[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
           {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1},
           {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1}
    };

    TileMaps[0][0].Tiles = (uint32*)Tiles00;

    TileMaps[0][1] = TileMaps[0][0];
    TileMaps[0][1].Tiles = (uint32*)Tiles01;

    TileMaps[1][0] = TileMaps[0][0];
    TileMaps[1][0].Tiles = (uint32*)Tiles10;

    TileMaps[1][1] = TileMaps[0][0];
    TileMaps[1][1].Tiles = (uint32*)Tiles11;

    world World;
    World.CountX = WORLD_COUNT_X;
    World.CountY = WORLD_COUNT_Y;
    World.TileMaps = (tile_map*)TileMaps;

    tile_map* TileMap = &TileMaps[0][0];

    real32 PlayerWidth = 0.75f * TileMap->TileWidth;
    real32 PlayerHeight = 1.0f * TileMap->TileHeight;

    for (int32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex) {
        game_controller_input* Controller = &(Input->Controllers[ControllerIndex]);

        real32 PlayerSpeed = 100 * Input->dtForFrame;

        real32 dPlayerX = 0;
        real32 dPlayerY = 0;

        if (Controller->MoveUp.EndedDown) {
            dPlayerY = -1;
        }
        if (Controller->MoveDown.EndedDown) {
            dPlayerY = 1;
        }
        if (Controller->MoveLeft.EndedDown) {
            dPlayerX = -1;
        }
        if (Controller->MoveRight.EndedDown) {
            dPlayerX = 1;
        }

        real32 NewPlayerX = GameState->PlayerX + dPlayerX * PlayerSpeed;
        real32 NewPlayerY = GameState->PlayerY + dPlayerY * PlayerSpeed;

        if (IsTileMapPointEmpty(TileMap, NewPlayerX, NewPlayerY) &&
            IsTileMapPointEmpty(TileMap, NewPlayerX - 0.5f * PlayerWidth, NewPlayerY) &&
            IsTileMapPointEmpty(TileMap, NewPlayerX + 0.5f * PlayerWidth, NewPlayerY)) {
            GameState->PlayerX = NewPlayerX;
            GameState->PlayerY = NewPlayerY;
        }
    }

    DrawRectangle(Buffer, 0, 0, (real32)Buffer->Width, (real32)Buffer->Height, 1.0f, 0.0f, 1.0f);

    for (int32 Row = 0; Row < TILE_MAP_COUNT_Y; ++Row) {
        for (int32 Column = 0; Column < TILE_MAP_COUNT_X; ++Column) {
            uint32 TileId = GetTileValueUnchecked(TileMap, Column, Row);
            real32 Color = 0.5f;
            if (TileId == 1) {
                Color = 1.0f;
            }

            real32 MinX = TileMap->UpperLeftX + ((real32)(Column)) * TileMap->TileWidth;
            real32 MinY = TileMap->UpperLeftY + ((real32)(Row)) * TileMap->TileHeight;
            real32 MaxX = MinX + TileMap->TileWidth;
            real32 MaxY = MinY + TileMap->TileHeight;

            DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Color, Color, Color);
        }
    }

    real32 PlayerR = 1;
    real32 PlayerG = 1;
    real32 PlayerB = 0;


    real32 PlayerMinX = GameState->PlayerX - 0.5f * PlayerWidth;
    real32 PlayerMinY = GameState->PlayerY - PlayerHeight;
    DrawRectangle(
        Buffer,
        PlayerMinX, PlayerMinY, PlayerMinX + PlayerWidth, PlayerMinY + PlayerHeight,
        PlayerR, PlayerG, PlayerB
    );
}
