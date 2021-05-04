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

struct tile_chunk {
    uint32* Tiles;
};

struct world {
    uint32 ChunkShift;
    uint32 ChunkMask;
    uint32 ChunkDim;

    real32 TileSideInMeters;
    int32 TileSideInPixels;

    real32 MetersToPixels;

    int32 TileChunkCountX;
    int32 TileChunkCountY;

    tile_chunk* TileChunks;
};

internal inline uint32 GetTileChunkValueUnchecked(world* World, tile_chunk* TileChunk, uint32 TileX, uint32 TileY) {
    Assert(TileChunk != 0);
    Assert(TileX < World->ChunkDim);
    Assert(TileY < World->ChunkDim);
    return TileChunk->Tiles[TileY * World->ChunkDim + TileX];
}

internal inline tile_chunk* GetTileChunk(world* World, int32 TileChunkX, int32 TileChunkY) {
    tile_chunk* TileChunk = 0;
    if ((TileChunkX >= 0) && (TileChunkX < World->TileChunkCountX) &&
        (TileChunkY >= 0) && (TileChunkY < World->TileChunkCountY)) {
        TileChunk = &World->TileChunks[TileChunkY * World->TileChunkCountX + TileChunkX];
    }
    return TileChunk;
}

internal uint32 GetTileChunkValue(
    world* World,
    tile_chunk* TileChunk,
    uint32 TestTileX, uint32 TestTileY
) {
    if (TileChunk == 0) {
        return 0;
    }

    return GetTileChunkValueUnchecked(World, TileChunk, TestTileX, TestTileY);
}

struct raw_position {
    int32 TileMapX;
    int32 TileMapY;

    real32 XTilemapRel;
    real32 YTilemapRel;
};

internal inline void CanonicalizeCoord(world* World, uint32* Tile, real32* TileRel) {
    int32 Offset = FloorReal32ToInt32((*TileRel) / World->TileSideInMeters);

    *Tile += Offset;
    *TileRel -= Offset * World->TileSideInMeters;

    Assert(*TileRel >= 0);
    Assert(*TileRel <= World->TileSideInMeters); //! Sometimes it's right on
}

internal inline world_position RecanonicalizePosition(world* World, world_position Pos) {
    world_position Result = Pos;

    CanonicalizeCoord(World, &Result.AbsTileX, &Result.XTileRel);
    CanonicalizeCoord(World, &Result.AbsTileY, &Result.YTileRel);

    return Result;
}

internal inline tile_chunk_position GetChunkPositionFor(world* World, uint32 AbsTileX, uint32 AbsTileY) {
    tile_chunk_position Result;
    Result.TileChunkX = AbsTileX >> World->ChunkShift;
    Result.TileChunkY = AbsTileY >> World->ChunkShift;
    Result.RelTileX = AbsTileX & World->ChunkMask;
    Result.RelTileY = AbsTileY & World->ChunkMask;
    return Result;
}

internal uint32 GetTileValue(world* World, uint32 AbsTileX, uint32 AbsTileY) {
    tile_chunk_position ChunkPos = GetChunkPositionFor(World, AbsTileX, AbsTileY);
    tile_chunk* TileChunk = GetTileChunk(World, ChunkPos.TileChunkX, ChunkPos.TileChunkY);
    uint32 TileValue = GetTileChunkValue(World, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY);
    return TileValue;
}

internal bool32 IsWorldPointEmpty(world* World, world_position CanonicalPos) {
    uint32 TileValue = GetTileValue(World, CanonicalPos.AbsTileX, CanonicalPos.AbsTileY);
    return TileValue == 0;
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

        Memory->IsInitialized = true;
    }

#define TILES_PER_CHUNK_COUNT 256

    world World;
    World.ChunkShift = 8;
    World.ChunkMask = 0xFF;
    World.ChunkDim = 256;

    World.TileSideInMeters = 1.4f;
    World.TileSideInPixels = 60;

    World.MetersToPixels = World.TileSideInPixels / (real32)World.TileSideInMeters;

    real32 LowerLeftX = -(real32)World.TileSideInPixels / 2;
    real32 LowerLeftY = (real32)Buffer->Height;

    real32 PlayerHeight = 1.4f;
    real32 PlayerWidth = 0.75f * PlayerHeight;

    uint32 Tiles[TILES_PER_CHUNK_COUNT][TILES_PER_CHUNK_COUNT] = {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
    };

    tile_chunk TileChunk = {};
    TileChunk.Tiles = (uint32*)Tiles;
    World.TileChunks = &TileChunk;
    World.TileChunkCountX = 1;
    World.TileChunkCountY = 1;

    for (int32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex) {
        game_controller_input* Controller = &(Input->Controllers[ControllerIndex]);

        real32 PlayerSpeed = 3.0f * Input->dtForFrame;

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

    real32 CenterX = (real32)Buffer->Width * 0.5f;
    real32 CenterY = (real32)Buffer->Height * 0.5f;

    for (int32 RelRow = -10; RelRow < 10; ++RelRow) {
        for (int32 RelColumn = -20; RelColumn < 20; ++RelColumn) {

            uint32 Column = RelColumn + GameState->PlayerP.AbsTileX;
            uint32 Row = RelRow + GameState->PlayerP.AbsTileY;

            uint32 TileId = GetTileValue(&World, Column, Row);
            real32 Color = 0.5f;
            if (TileId == 1) {
                Color = 1.0f;
            }

            if (Row == GameState->PlayerP.AbsTileY && Column == GameState->PlayerP.AbsTileX) {
                Color = 0.0f;
            }

            real32 MinX = CenterX + ((real32)(RelColumn)) * World.TileSideInPixels;
            real32 MinY = CenterY - ((real32)(RelRow)) * World.TileSideInPixels;
            real32 MaxX = MinX + World.TileSideInPixels;
            real32 MaxY = MinY - World.TileSideInPixels;

            DrawRectangle(Buffer, MinX, MaxY, MaxX, MinY, Color, Color, Color);
        }
    }

    real32 PlayerR = 1;
    real32 PlayerG = 1;
    real32 PlayerB = 0;

    real32 PlayerMinX = CenterX + World.MetersToPixels * GameState->PlayerP.XTileRel - World.MetersToPixels * 0.5f * PlayerWidth;
    real32 PlayerMinY = CenterY - World.MetersToPixels * GameState->PlayerP.YTileRel - World.MetersToPixels * PlayerHeight;
    DrawRectangle(
        Buffer,
        PlayerMinX, PlayerMinY,
        PlayerMinX + World.MetersToPixels * PlayerWidth, PlayerMinY + World.MetersToPixels * PlayerHeight,
        PlayerR, PlayerG, PlayerB
    );
}
