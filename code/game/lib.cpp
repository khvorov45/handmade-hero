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

internal inline int32 FloorReal32ToInt32(real32 Real32) {
    return (int32)floorf(Real32);
}

struct tile_map {
    uint32* Tiles;
};

struct world {
    int32 TilesPerMapCountX;
    int32 TilesPerMapCountY;

    real32 UpperLeftX;
    real32 UpperLeftY;

    real32 TileWidth;
    real32 TileHeight;

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

struct canonical_position {
    int32 TileMapX;
    int32 TileMapY;

    int32 TileX;
    int32 TileY;

    //* Tile-relative
    real32 X;
    real32 Y;
};

struct raw_position {
    int32 TileMapX;
    int32 TileMapY;

    //* Tilemap-relative
    real32 X;
    real32 Y;
};

internal inline canonical_position GetCanonicalPosition(world* World, raw_position Pos) {
    canonical_position CanonicalPos;
    CanonicalPos.TileMapX = Pos.TileMapX;
    CanonicalPos.TileMapY = Pos.TileMapY;

    real32 X = Pos.X - World->UpperLeftX;
    real32 Y = Pos.Y - World->UpperLeftY;
    CanonicalPos.TileX = FloorReal32ToInt32(X / World->TileWidth);
    CanonicalPos.TileY = FloorReal32ToInt32(Y / World->TileHeight);
    CanonicalPos.X = X - CanonicalPos.TileX * World->TileWidth;
    CanonicalPos.Y = Y - CanonicalPos.TileY * World->TileHeight;

    Assert(CanonicalPos.X >= 0);
    Assert(CanonicalPos.Y >= 0);
    Assert(CanonicalPos.X < World->TileWidth);
    Assert(CanonicalPos.Y < World->TileHeight);

    if (CanonicalPos.TileX < 0) {
        CanonicalPos.TileX += World->TilesPerMapCountX;
        --CanonicalPos.TileMapX;
    }
    if (CanonicalPos.TileY < 0) {
        CanonicalPos.TileY += World->TilesPerMapCountY;
        --CanonicalPos.TileMapY;
    }

    if (CanonicalPos.TileX >= World->TilesPerMapCountX) {
        CanonicalPos.TileX -= World->TilesPerMapCountX;
        ++CanonicalPos.TileMapX;
    }
    if (CanonicalPos.TileY >= World->TilesPerMapCountY) {
        CanonicalPos.TileY -= World->TilesPerMapCountY;
        ++CanonicalPos.TileMapY;
    }

    return CanonicalPos;
}

internal bool32 IsWorldPointEmpty(world* World, raw_position TestPos) {
    bool32 IsEmpty = false;

    canonical_position CanonicalPos = GetCanonicalPosition(World, TestPos);

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
        GameState->PlayerX = 300;
        GameState->PlayerY = 300;
        GameState->PlayerTileMapX = 0;
        GameState->PlayerTileMapY = 0;
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
    World.UpperLeftX = -30;
    World.UpperLeftY = 0;
    World.TileWidth = 60;
    World.TileHeight = 60;

    tile_map TileMaps[WORLD_TILEMAP_COUNT_Y][WORLD_TILEMAP_COUNT_X];

    World.TileMaps = (tile_map*)TileMaps;

    real32 PlayerWidth = 0.75f * World.TileWidth;
    real32 PlayerHeight = 1.0f * World.TileHeight;

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

        real32 PlayerSpeed = 150 * Input->dtForFrame;

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

        raw_position PlayerPos = {
            GameState->PlayerTileMapX, GameState->PlayerTileMapY, NewPlayerX, NewPlayerY
        };
        raw_position PlayerLeft = PlayerPos;
        PlayerLeft.X -= 0.5f * PlayerWidth;
        raw_position PlayerRight = PlayerPos;
        PlayerRight.X += 0.5f * PlayerWidth;

        if (IsWorldPointEmpty(&World, PlayerPos) &&
            IsWorldPointEmpty(&World, PlayerLeft) &&
            IsWorldPointEmpty(&World, PlayerRight)) {

            canonical_position CanonicalPos = GetCanonicalPosition(&World, PlayerPos);

            GameState->PlayerTileMapX = CanonicalPos.TileMapX;
            GameState->PlayerTileMapY = CanonicalPos.TileMapY;
            GameState->PlayerX = World.UpperLeftX + CanonicalPos.TileX * World.TileWidth + CanonicalPos.X;
            GameState->PlayerY = World.UpperLeftY + CanonicalPos.TileY * World.TileHeight + CanonicalPos.Y;
        }
    }

    DrawRectangle(Buffer, 0, 0, (real32)Buffer->Width, (real32)Buffer->Height, 1.0f, 0.0f, 1.0f);

    tile_map* TileMap = GetTilemap(&World, GameState->PlayerTileMapX, GameState->PlayerTileMapY);
    for (int32 Row = 0; Row < World.TilesPerMapCountY; ++Row) {
        for (int32 Column = 0; Column < World.TilesPerMapCountX; ++Column) {
            uint32 TileId = GetTileValueUnchecked(&World, TileMap, Column, Row);
            real32 Color = 0.5f;
            if (TileId == 1) {
                Color = 1.0f;
            }

            real32 MinX = World.UpperLeftX + ((real32)(Column)) * World.TileWidth;
            real32 MinY = World.UpperLeftY + ((real32)(Row)) * World.TileHeight;
            real32 MaxX = MinX + World.TileWidth;
            real32 MaxY = MinY + World.TileHeight;

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
