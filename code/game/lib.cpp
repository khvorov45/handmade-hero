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

struct tile_map {
    uint32* Tiles;
};

struct world {
    real32 TileSideInMeters;
    int32 TileSideInPixels;

    real32 MetersToPixels;

    int32 TilesPerMapCountX;
    int32 TilesPerMapCountY;

    real32 UpperLeftX;
    real32 UpperLeftY;

    int32 TilemapCountX;
    int32 TilemapCountY;

    tile_map* TileMaps;
};

internal inline uint32 GetTileValueUnchecked(world* World, tile_map* TileMap, int32 TileX, int32 TileY) {
    Assert(TileMap != 0);
    Assert(
        (TileX >= 0) && (TileX < World->TilesPerMapCountX) &&
        (TileY >= 0) && (TileY < World->TilesPerMapCountY)
    );
    return TileMap->Tiles[TileY * World->TilesPerMapCountX + TileX];
}

internal inline tile_map* GetTilemap(world* World, int32 TileMapX, int32 TileMapY) {
    tile_map* TileMap = 0;
    if ((TileMapX >= 0) && (TileMapX < World->TilemapCountX) &&
        (TileMapY >= 0) && (TileMapY < World->TilemapCountY)) {
        TileMap = &World->TileMaps[TileMapY * World->TilemapCountX + TileMapX];
    }
    return TileMap;
}

internal bool32 IsTileMapTileEmpty(
    world* World,
    tile_map* TileMap,
    int32 TestTileX, int32 TestTileY
) {
    bool32 IsEmpty = false;
    if (TileMap == 0) {
        return IsEmpty;
    }

    if ((TestTileX >= 0) && (TestTileX < World->TilesPerMapCountX) &&
        (TestTileY >= 0) && (TestTileY < World->TilesPerMapCountY)) {
        uint32 TileMapValue = GetTileValueUnchecked(World, TileMap, TestTileX, TestTileY);
        IsEmpty = TileMapValue == 0;
    }

    return IsEmpty;
}

struct raw_position {
    int32 TileMapX;
    int32 TileMapY;

    real32 XTilemapRel;
    real32 YTilemapRel;
};

internal inline void CanonicalizeCoord(world* World, int32 TileCount, int32* TileMap, int32* Tile, real32* TileRel) {
    int32 Offset = FloorReal32ToInt32((*TileRel) / World->TileSideInMeters);
    *Tile += Offset;
    *TileRel -= Offset * World->TileSideInMeters;

    Assert(*TileRel >= 0);
    Assert(*TileRel <= World->TileSideInMeters); //! Sometimes it's right on

    if (*Tile < 0) {
        *Tile += TileCount;
        --* TileMap;
    }

    if (*Tile >= TileCount) {
        *Tile -= TileCount;
        ++* TileMap;
    }
}

internal inline world_position RecanonicalizePosition(world* World, world_position Pos) {
    world_position Result = Pos;

    CanonicalizeCoord(World, World->TilesPerMapCountX, &Result.TileMapX, &Result.TileX, &Result.XTileRel);
    CanonicalizeCoord(World, World->TilesPerMapCountY, &Result.TileMapY, &Result.TileY, &Result.YTileRel);

    return Result;
}

internal bool32 IsWorldPointEmpty(world* World, world_position CanonicalPos) {
    bool32 IsEmpty = false;

    tile_map* TileMap = GetTilemap(World, CanonicalPos.TileMapX, CanonicalPos.TileMapY);

    return IsTileMapTileEmpty(World, TileMap, CanonicalPos.TileX, CanonicalPos.TileY);
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
        GameState->PlayerP.TileMapX = 0;
        GameState->PlayerP.TileMapY = 0;
        GameState->PlayerP.TileX = 3;
        GameState->PlayerP.TileY = 3;
        GameState->PlayerP.XTileRel = 5.0f;
        GameState->PlayerP.YTileRel = 5.5f;

        Memory->IsInitialized = true;
    }

#define WORLD_TILEMAP_COUNT_X 2
#define WORLD_TILEMAP_COUNT_Y 2

#define TILES_PER_MAP_COUNT_X 17
#define TILES_PER_MAP_COUNT_Y 9

    world World;
    World.TilemapCountX = WORLD_TILEMAP_COUNT_X;
    World.TilemapCountY = WORLD_TILEMAP_COUNT_Y;
    World.TilesPerMapCountX = TILES_PER_MAP_COUNT_X;
    World.TilesPerMapCountY = TILES_PER_MAP_COUNT_Y;

    World.TileSideInMeters = 1.4f;
    World.TileSideInPixels = 60;

    World.MetersToPixels = World.TileSideInPixels / (real32)World.TileSideInMeters;

    World.UpperLeftX = -(real32)World.TileSideInPixels / 2;
    World.UpperLeftY = 0;

    tile_map TileMaps[WORLD_TILEMAP_COUNT_Y][WORLD_TILEMAP_COUNT_X];

    World.TileMaps = (tile_map*)TileMaps;

    real32 PlayerHeight = 1.4f;
    real32 PlayerWidth = 0.75f * PlayerHeight;

    uint32 Tiles00[TILES_PER_MAP_COUNT_Y][TILES_PER_MAP_COUNT_X] = {
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
    uint32 Tiles01[TILES_PER_MAP_COUNT_Y][TILES_PER_MAP_COUNT_X] = {
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
    uint32 Tiles10[TILES_PER_MAP_COUNT_Y][TILES_PER_MAP_COUNT_X] = {
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
    uint32 Tiles11[TILES_PER_MAP_COUNT_Y][TILES_PER_MAP_COUNT_X] = {
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

    TileMaps[0][1].Tiles = (uint32*)Tiles10;

    TileMaps[1][0].Tiles = (uint32*)Tiles01;

    TileMaps[1][1].Tiles = (uint32*)Tiles11;

    for (int32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex) {
        game_controller_input* Controller = &(Input->Controllers[ControllerIndex]);

        real32 PlayerSpeed = 2.0f * Input->dtForFrame;

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

        world_position NewPlayerP = GameState->PlayerP;

        NewPlayerP.XTileRel += dPlayerX * PlayerSpeed;
        NewPlayerP.YTileRel += dPlayerY * PlayerSpeed;
        NewPlayerP = RecanonicalizePosition(&World, NewPlayerP);

        world_position PlayerLeft = NewPlayerP;
        PlayerLeft.XTileRel -= 0.5f * PlayerWidth;
        PlayerLeft = RecanonicalizePosition(&World, PlayerLeft);

        world_position PlayerRight = NewPlayerP;
        PlayerRight.XTileRel += 0.5f * PlayerWidth;
        PlayerRight = RecanonicalizePosition(&World, PlayerRight);

        if (IsWorldPointEmpty(&World, NewPlayerP) &&
            IsWorldPointEmpty(&World, PlayerLeft) &&
            IsWorldPointEmpty(&World, PlayerRight)) {
            GameState->PlayerP = NewPlayerP;
        }
    }

    DrawRectangle(Buffer, 0, 0, (real32)Buffer->Width, (real32)Buffer->Height, 1.0f, 0.0f, 1.0f);

    tile_map* TileMap = GetTilemap(&World, GameState->PlayerP.TileMapX, GameState->PlayerP.TileMapY);
    for (int32 Row = 0; Row < World.TilesPerMapCountY; ++Row) {
        for (int32 Column = 0; Column < World.TilesPerMapCountX; ++Column) {
            uint32 TileId = GetTileValueUnchecked(&World, TileMap, Column, Row);
            real32 Color = 0.5f;
            if (TileId == 1) {
                Color = 1.0f;
            }

            if (Row == GameState->PlayerP.TileY && Column == GameState->PlayerP.TileX) {
                Color = 0.0f;
            }

            real32 MinX = World.UpperLeftX + ((real32)(Column)) * World.TileSideInPixels;
            real32 MinY = World.UpperLeftY + ((real32)(Row)) * World.TileSideInPixels;
            real32 MaxX = MinX + World.TileSideInPixels;
            real32 MaxY = MinY + World.TileSideInPixels;

            DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Color, Color, Color);
        }
    }

    real32 PlayerR = 1;
    real32 PlayerG = 1;
    real32 PlayerB = 0;

    real32 PlayerMinX = World.UpperLeftX + World.TileSideInPixels * GameState->PlayerP.TileX + World.MetersToPixels * GameState->PlayerP.XTileRel - World.MetersToPixels * 0.5f * PlayerWidth;
    real32 PlayerMinY = World.UpperLeftY + World.TileSideInPixels * GameState->PlayerP.TileY + World.MetersToPixels * GameState->PlayerP.YTileRel - World.MetersToPixels * PlayerHeight;
    DrawRectangle(
        Buffer,
        PlayerMinX, PlayerMinY,
        PlayerMinX + World.MetersToPixels * PlayerWidth, PlayerMinY + World.MetersToPixels * PlayerHeight,
        PlayerR, PlayerG, PlayerB
    );
}
