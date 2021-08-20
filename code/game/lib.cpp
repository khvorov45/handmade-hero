#include "lib.hpp"
#include "world.cpp"
#include "sim_region.cpp"
#include "render_group.cpp"
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

struct add_low_entity_result_ {
    low_entity_* Low;
    uint32 LowIndex;
};

internal add_low_entity_result_
AddLowEntity_(game_state* GameState, entity_type EntityType, world_position Position) {

    Assert(GameState->LowEntity_Count < ArrayCount(GameState->LowEntities));

    add_low_entity_result_ Result = {};

    Result.LowIndex = GameState->LowEntity_Count++;

    Result.Low = GameState->LowEntities + Result.LowIndex;
    *Result.Low = {};
    Result.Low->Sim.Type = EntityType;
    Result.Low->P = NullPosition();
    Result.Low->Sim.Collision = GameState->NullCollision;

    ChangeEntityLocation(
        &GameState->WorldArena, GameState->World, Result.LowIndex, Result.Low, Position
    );

    return Result;
}

internal add_low_entity_result_ AddGroundedEntity(
    game_state* GameState,
    entity_type EntityType,
    world_position Position,
    sim_entity_collision_volume_group* Collision
) {
    add_low_entity_result_ Result = AddLowEntity_(GameState, EntityType, Position);
    Result.Low->Sim.Collision = Collision;
    return Result;
}

internal void InitHitpoints_(low_entity_* EntityLow, uint32 Max) {
    Assert(Max <= ArrayCount(EntityLow->Sim.HitPoint));
    EntityLow->Sim.HitPointMax = Max;
    for (uint32 HealthIndex = 0; HealthIndex < Max; HealthIndex++) {
        EntityLow->Sim.HitPoint[HealthIndex].FilledAmount = HITPOINT_SUBCOUNT;
    }
}

internal add_low_entity_result_ AddSword_(game_state* GameState) {

    add_low_entity_result_ Entity = AddLowEntity_(GameState, EntityType_Sword, NullPosition());

    Entity.Low->Sim.Collision = GameState->SwordCollision;

    AddFlags(&Entity.Low->Sim, EntityFlag_Moveable);

    return Entity;
}

internal add_low_entity_result_ AddPlayer_(game_state* GameState) {

    add_low_entity_result_ Entity =
        AddGroundedEntity(GameState, EntityType_Hero, GameState->CameraP, GameState->PlayerCollision);

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides | EntityFlag_Moveable);

    InitHitpoints_(Entity.Low, 3);

    add_low_entity_result_ Sword = AddSword_(GameState);
    Entity.Low->Sim.Sword.Index = Sword.LowIndex;

    if (GameState->CameraFollowingEntityIndex == 0) {
        GameState->CameraFollowingEntityIndex = Entity.LowIndex;
    }

    return Entity;
}

internal add_low_entity_result_
AddWall_(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {

    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity =
        AddGroundedEntity(GameState, EntityType_Wall, EntityLowP, GameState->WallCollision);

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides);

    return Entity;
}

internal add_low_entity_result_
AddMonster_(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {

    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity =
        AddGroundedEntity(GameState, EntityType_Monster, EntityLowP, GameState->MonsterCollision);

    InitHitpoints_(Entity.Low, 3);

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides | EntityFlag_Moveable);

    return Entity;
}

internal add_low_entity_result_
AddFamiliar_(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {

    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity =
        AddGroundedEntity(GameState, EntityType_Familiar, EntityLowP, GameState->FamiliarCollision);

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides | EntityFlag_Moveable);

    return Entity;
}

internal add_low_entity_result_
AddStair(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {

    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity =
        AddGroundedEntity(GameState, EntityType_Stairwell, EntityLowP, GameState->StairCollision);

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides);

    Entity.Low->Sim.WalkableHeight = GameState->TypicalFloorHeight;
    Entity.Low->Sim.WalkableDim = Entity.Low->Sim.Collision->TotalVolume.Dim.xy;

    return Entity;
}

internal add_low_entity_result_ AddStandardRoom(
    game_state* GameState,
    uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ
) {

    world_position EntityLowP =
        ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);

    add_low_entity_result_ Entity =
        AddGroundedEntity(GameState, EntityType_Space, EntityLowP, GameState->StandardRoomCollision);

    AddFlags(&Entity.Low->Sim, EntityFlag_Traversable);

    return Entity;
}

internal void DrawHitpoints_(sim_entity* Entity, render_group* RenderGroup) {
    if (Entity->HitPointMax >= 1) {
        v2 HealthDim = { 0.2f, 0.2f };
        real32 SpacingX = 2.0f * HealthDim.x;
        v2 HitP = { (Entity->HitPointMax - 1) * (-0.5f) * SpacingX, -0.2f };
        v2 dHitP = { SpacingX, 0.0f };
        for (uint32 HealthIndex = 0; HealthIndex < Entity->HitPointMax; ++HealthIndex) {
            v4 Color = { 1.0f, 0.0f, 0.0f, 1.0f };
            hit_point* HitPoint = Entity->HitPoint + HealthIndex;
            if (HitPoint->FilledAmount == 0) {
                Color.r = 0.2f;
                Color.g = 0.2f;
                Color.b = 0.2f;
            }
            PushRect(RenderGroup, HitP, 0, HealthDim, Color, 0.0f);
            HitP += dHitP;
        }
    }
}

sim_entity_collision_volume_group*
MakeSimpleGroundedCollision(game_state* GameState, real32 DimX, real32 DimY, real32 DimZ) {
    sim_entity_collision_volume_group* Group =
        PushStruct(&GameState->WorldArena, sim_entity_collision_volume_group);
    Group->VolumeCount = 1;
    Group->Volumes =
        PushArray(&GameState->WorldArena, Group->VolumeCount, sim_entity_collision_volume);
    Group->TotalVolume.OffsetP = V3(0, 0, 0.5f * DimZ);
    Group->TotalVolume.Dim = V3(DimX, DimY, DimZ);
    Group->Volumes[0] = Group->TotalVolume;
    return Group;
}

sim_entity_collision_volume_group*
MakeNullCollision(game_state* GameState) {
    sim_entity_collision_volume_group* Group =
        PushStruct(&GameState->WorldArena, sim_entity_collision_volume_group);
    Group->VolumeCount = 0;
    Group->Volumes = 0;
    Group->TotalVolume.OffsetP = V3(0, 0, 0);
    Group->TotalVolume.Dim = V3(0, 0, 0);
    return Group;
}

internal void ClearBitmap(loaded_bitmap* Bitmap) {
    if (Bitmap->Memory) {
        int32 TotalBitmapSize = Bitmap->Width * Bitmap->Height * BITMAP_BYTES_PER_PIXEL;
        ZeroSize(TotalBitmapSize, Bitmap->Memory);
    }
}

internal loaded_bitmap
MakeEmptyBitmap(memory_arena* Arena, int32 Width, int32 Height, bool32 ClearToZero = true) {

    loaded_bitmap Result = {};

    Result.Width = Width;
    Result.Height = Height;
    Result.Pitch = Width * BITMAP_BYTES_PER_PIXEL;

    int32 TotalBitmapSize = Width * Height * BITMAP_BYTES_PER_PIXEL;
    Result.Memory = PushSize(Arena, TotalBitmapSize);

    if (ClearToZero) {
        ClearBitmap(&Result);
    }

    return Result;
}

internal void MakeSphereNormalMap(loaded_bitmap* Bitmap, real32 Roughness, real32 Cx = 1.0f, real32 Cy = 1.0f) {
    real32 InvWidth = 1.0f / ((real32)Bitmap->Width - 1.0f);
    real32 InvHeight = 1.0f / ((real32)Bitmap->Height - 1.0f);
    uint8* Row = (uint8*)Bitmap->Memory;
    for (int32 Y = 0; Y < Bitmap->Height; Y++) {
        uint32* Pixel = (uint32*)Row;
        for (int32 X = 0; X < Bitmap->Width; X++) {
            v2 BitmapUV = V2(InvWidth * (real32)X, InvHeight * (real32)Y);
            v3 Normal = V3(0.0f, 0.707f, 0.707f);
            real32 Nx = Cx * (2.0f * BitmapUV.x - 1.0f);
            real32 Ny = Cy * (2.0f * BitmapUV.y - 1.0f);
            real32 RootTerm = 1.0f - Square(Nx) - Square(Ny);
            if (RootTerm >= 0.0f) {
                real32 Nz = SquareRoot(RootTerm);
                Normal = V3(Nx, Ny, Nz);
            }
            v4 Color = V4(
                (Normal.x + 1) * 0.5f * 255.0f,
                (Normal.y + 1) * 0.5f * 255.0f,
                (Normal.z + 1) * 0.5f * 255.0f,
                Roughness * 255.0f
            );
            *Pixel++ =
                (RoundReal32ToUint32(Color.a) << 24) |
                (RoundReal32ToUint32(Color.r) << 16) |
                (RoundReal32ToUint32(Color.g) << 8) |
                (RoundReal32ToUint32(Color.b));;
        }
        Row += Bitmap->Pitch;
    }
}

internal void MakeSphereDiffuseMap(loaded_bitmap* Bitmap, real32 Cx = 1.0f, real32 Cy = 1.0f) {
    real32 InvWidth = 1.0f / ((real32)Bitmap->Width - 1.0f);
    real32 InvHeight = 1.0f / ((real32)Bitmap->Height - 1.0f);
    uint8* Row = (uint8*)Bitmap->Memory;
    for (int32 Y = 0; Y < Bitmap->Height; Y++) {
        uint32* Pixel = (uint32*)Row;
        for (int32 X = 0; X < Bitmap->Width; X++) {
            v2 BitmapUV = V2(InvWidth * (real32)X, InvHeight * (real32)Y);
            real32 Nx = Cx * (2.0f * BitmapUV.x - 1.0f);
            real32 Ny = Cy * (2.0f * BitmapUV.y - 1.0f);
            real32 Alpha = 0.0f;
            real32 RootTerm = 1.0f - Square(Nx) - Square(Ny);
            if (RootTerm >= 0.0f) {
                Alpha = 1.0f;
            }
            v3 BaseColor = V3(1.0f, 1.0f, 1.0f);
            Alpha *= 255.0f;
            v4 Color = V4(
                Alpha * BaseColor.x,
                Alpha * BaseColor.y,
                Alpha * BaseColor.z,
                Alpha
            );
            *Pixel++ =
                (RoundReal32ToUint32(Color.a) << 24) |
                (RoundReal32ToUint32(Color.r) << 16) |
                (RoundReal32ToUint32(Color.g) << 8) |
                (RoundReal32ToUint32(Color.b));;
        }
        Row += Bitmap->Pitch;
    }
}

internal void MakePyramidNormalMap(loaded_bitmap* Bitmap, real32 Roughness) {
    real32 InvWidth = 1.0f / ((real32)Bitmap->Width - 1.0f);
    real32 InvHeight = 1.0f / ((real32)Bitmap->Height - 1.0f);
    uint8* Row = (uint8*)Bitmap->Memory;
    for (int32 Y = 0; Y < Bitmap->Height; Y++) {
        uint32* Pixel = (uint32*)Row;
        for (int32 X = 0; X < Bitmap->Width; X++) {
            v2 BitmapUV = V2(InvWidth * (real32)X, InvHeight * (real32)Y);
            int32 InvX = Bitmap->Width - 1 - X;
            v3 Normal = V3(0.0f, 0.0f, 0.707f);
            if (X < Y) {
                if (InvX < Y) {
                    Normal.x = -0.707f;
                } else {
                    Normal.y = 0.707f;
                }
            } else {
                if (InvX < Y) {
                    Normal.y = -0.707f;
                } else {
                    Normal.x = 0.707f;
                }
            }
            v4 Color = V4(
                (Normal.x + 1) * 0.5f * 255.0f,
                (Normal.y + 1) * 0.5f * 255.0f,
                (Normal.z + 1) * 0.5f * 255.0f,
                Roughness * 255.0f
            );
            *Pixel++ =
                (RoundReal32ToUint32(Color.a) << 24) |
                (RoundReal32ToUint32(Color.r) << 16) |
                (RoundReal32ToUint32(Color.g) << 8) |
                (RoundReal32ToUint32(Color.b));;
        }
        Row += Bitmap->Pitch;
    }
}

internal void
FillGroundChunk(
    transient_state* TranState, game_state* GameState,
    ground_buffer* GroundBuffer, world_position* ChunkP
) {
    temporary_memory GroundMemory = BeginTemporaryMemory(&TranState->TranArena);
    render_group* GroundRenderGroup =
        AllocateRenderGroup(&TranState->TranArena, Megabytes(4), 1.0f);

    Clear(GroundRenderGroup, V4(1.0f, 1.0f, 0.0f, 1.0f));

    loaded_bitmap* Buffer = &GroundBuffer->Bitmap;

    real32 Width = (real32)Buffer->Width;
    real32 Height = (real32)Buffer->Height;

    GroundBuffer->P = *ChunkP;

    for (int32 ChunkOffsetY = -1; ChunkOffsetY <= 1; ChunkOffsetY++) {
        for (int32 ChunkOffsetX = -1; ChunkOffsetX <= 1; ChunkOffsetX++) {

            int32 ChunkX = ChunkP->ChunkX + ChunkOffsetX;
            int32 ChunkY = ChunkP->ChunkY + ChunkOffsetY;
            int32 ChunkZ = ChunkP->ChunkZ;

            random_series Series = RandomSeed(139 * ChunkX + 593 * ChunkY + 329 * ChunkZ);

            v2 Center = V2(ChunkOffsetX * Width, -ChunkOffsetY * Height);

            for (uint32 GrassIndex = 0; GrassIndex < 100; ++GrassIndex) {

                loaded_bitmap* Stamp;
                if (RandomChoice(&Series, 2)) {
                    Stamp = GameState->Grass + RandomChoice(&Series, ArrayCount(GameState->Grass));
                } else {
                    Stamp = GameState->Ground + RandomChoice(&Series, ArrayCount(GameState->Ground));
                }

                v2 BitmapCenter = 0.5f * V2i(Stamp->Width, Stamp->Height);

                v2 Offset = V2(Width * (RandomUnilateral(&Series)), Height * (RandomUnilateral(&Series)));

                v2 P = Center + Offset - BitmapCenter;
                PushBitmap(GroundRenderGroup, Stamp, P, 0, V2(0, 0));
            }
        }
    }

    for (int32 ChunkOffsetY = -1; ChunkOffsetY <= 1; ChunkOffsetY++) {
        for (int32 ChunkOffsetX = -1; ChunkOffsetX <= 1; ChunkOffsetX++) {

            int32 ChunkX = ChunkP->ChunkX + ChunkOffsetX;
            int32 ChunkY = ChunkP->ChunkY + ChunkOffsetY;
            int32 ChunkZ = ChunkP->ChunkZ;

            random_series Series = RandomSeed(139 * ChunkX + 593 * ChunkY + 329 * ChunkZ);

            v2 Center = V2(ChunkOffsetX * Width, -ChunkOffsetY * Height);

            for (uint32 GrassIndex = 0; GrassIndex < 50; ++GrassIndex) {

                loaded_bitmap* Stamp =
                    GameState->Tuft + RandomChoice(&Series, ArrayCount(GameState->Tuft));

                v2 BitmapCenter = 0.5f * V2i(Stamp->Width, Stamp->Height);

                v2 Offset = { Width * RandomUnilateral(&Series), Height * RandomUnilateral(&Series) };

                real32 Radius = 5.0f;
                v2 P = Center + Offset - BitmapCenter;
                PushBitmap(GroundRenderGroup, Stamp, P, 0, V2(0, 0));
            }
        }
    }

    RenderGroupToOutput(GroundRenderGroup, Buffer);
    EndTemporaryMemory(GroundMemory);
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

    uint32 GroundBufferWidth = 256;
    uint32 GroundBufferHeight = 256;

    if (!Memory->IsInitialized) {
        InitializeArena(
            &GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
            (uint8*)Memory->PermanentStorage + sizeof(game_state)
        );

        AddLowEntity_(GameState, EntityType_Null, NullPosition());

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world* TileMap = GameState->World;


        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;

        GameState->TypicalFloorHeight = 3.0f;
        GameState->MetersToPixels = 42.0f;
        GameState->PixelsToMeters = 1.0f / GameState->MetersToPixels;

        v3 WorldChunkDimInMeters = V3(
            GameState->PixelsToMeters * (real32)GroundBufferWidth,
            GameState->PixelsToMeters * (real32)GroundBufferHeight,
            GameState->TypicalFloorHeight
        );

        InitializeWorld(TileMap, WorldChunkDimInMeters);

        real32 TileSideInMeters = 1.4f;

        GameState->NullCollision = MakeNullCollision(GameState);
        GameState->SwordCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 0.1f);
        GameState->StairCollision = MakeSimpleGroundedCollision(
            GameState,
            TileSideInMeters,
            2.0f * TileSideInMeters,
            1.1f * GameState->TypicalFloorHeight
        );
        GameState->PlayerCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 1.2f);
        GameState->MonsterCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 0.5f);
        GameState->FamiliarCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 0.5f);
        GameState->WallCollision = MakeSimpleGroundedCollision(
            GameState, TileSideInMeters,
            TileSideInMeters,
            GameState->TypicalFloorHeight
        );
        GameState->StandardRoomCollision = MakeSimpleGroundedCollision(
            GameState,
            TilesPerWidth * TileSideInMeters,
            TilesPerHeight * TileSideInMeters,
            0.9f * GameState->TypicalFloorHeight
        );

        GameState->Grass[0] =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/grass00.bmp");
        GameState->Grass[1] =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/grass01.bmp");
        GameState->Tuft[0] =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/tuft00.bmp");
        GameState->Tuft[1] =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/tuft01.bmp");
        GameState->Tuft[2] =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/tuft02.bmp");
        GameState->Ground[0] =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/ground00.bmp");
        GameState->Ground[1] =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/ground01.bmp");
        GameState->Ground[2] =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/ground02.bmp");
        GameState->Ground[3] =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/ground03.bmp");

        GameState->Backdrop =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");
        GameState->HeroShadow =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_shadow.bmp");
        GameState->Tree =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/tree00.bmp");
        GameState->Sword =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/rock03.bmp");
        GameState->Stairwell =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/rock02.bmp");

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

        uint32 ScreenBaseX = 0;
        uint32 ScreenBaseY = 0;
        uint32 ScreenBaseZ = 0;

        uint32 ScreenX = ScreenBaseX;
        uint32 ScreenY = ScreenBaseY;
        uint32 AbsTileZ = ScreenBaseZ;

        random_series Series = RandomSeed(1234);

        bool32 DoorLeft = false;
        bool32 DoorRight = false;
        bool32 DoorTop = false;
        bool32 DoorBottom = false;
        bool32 DoorUp = false;
        bool32 DoorDown = false;

        for (uint32 ScreenIndex = 0; ScreenIndex < 2000; ++ScreenIndex) {

            uint32 DoorDirection = RandomChoice(&Series, DoorUp || DoorDown || true ? 2 : 3);

            bool32 CreatedZDoor = false;

            if (DoorDirection == 2) {
                CreatedZDoor = true;
                if (AbsTileZ == ScreenBaseZ) {
                    DoorUp = true;
                } else {
                    DoorDown = true;
                }
            } else if (DoorDirection == 1) {
                DoorRight = true;
            } else {
                DoorTop = true;
            }

            AddStandardRoom(
                GameState,
                ScreenX * TilesPerWidth + TilesPerWidth / 2,
                ScreenY * TilesPerHeight + TilesPerHeight / 2,
                AbsTileZ
            );

            for (uint32 TileY = 0; TileY < TilesPerHeight; ++TileY) {
                for (uint32 TileX = 0; TileX < TilesPerWidth; ++TileX) {
                    uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;

                    bool32 ShouldBeDoor = false;
                    if (TileX == 0 && (!DoorLeft || TileY != TilesPerHeight / 2)) {
                        ShouldBeDoor = true;
                    }
                    if (TileX == TilesPerWidth - 1 && (!DoorRight || TileY != TilesPerHeight / 2)) {
                        ShouldBeDoor = true;
                    }
                    if (TileY == 0 && (!DoorBottom || TileX != TilesPerWidth / 2)) {
                        ShouldBeDoor = true;
                    }
                    if (TileY == TilesPerHeight - 1 && (!DoorTop || TileX != TilesPerWidth / 2)) {
                        ShouldBeDoor = true;
                    }

                    if (ShouldBeDoor) {
                        AddWall_(GameState, AbsTileX, AbsTileY, AbsTileZ);
                    } else if (CreatedZDoor) {
                        if (TileX == 10 && TileY == 5) {
                            AddStair(
                                GameState, AbsTileX, AbsTileY, DoorDown ? AbsTileZ - 1 : AbsTileZ
                            );
                        }
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

            if (DoorDirection == 2) {
                if (AbsTileZ == ScreenBaseZ) {
                    AbsTileZ = ScreenBaseZ + 1;
                } else {
                    AbsTileZ = ScreenBaseZ;
                }
            } else if (DoorDirection == 1) {
                ScreenX += 1;
            } else {
                ScreenY += 1;
            }

        }

        uint32 CameraTileX = ScreenBaseX * TilesPerWidth + 17 / 2;
        uint32 CameraTileY = ScreenBaseY * TilesPerHeight + 9 / 2;
        uint32 CameraTileZ = ScreenBaseZ;

        AddMonster_(GameState, CameraTileX - 3, CameraTileY + 2, CameraTileZ);
        AddFamiliar_(GameState, CameraTileX - 2, CameraTileY - 2, CameraTileZ);

        world_position NewCameraP =
            ChunkPositionFromTilePosition(GameState->World, CameraTileX, CameraTileY, CameraTileZ);
        GameState->CameraP = NewCameraP;


        Memory->IsInitialized = true;
    }

    Assert(sizeof(transient_state) <= Memory->TransientStorageSize);

    transient_state* TranState = (transient_state*)Memory->TransientStorage;

    if (!TranState->IsInitialized) {
        InitializeArena(
            &TranState->TranArena, Memory->TransientStorageSize - sizeof(transient_state),
            (uint8*)Memory->TransientStorage + sizeof(transient_state)
        );

        TranState->GroundBufferCount = 64;
        TranState->GroundBuffers =
            PushArray(&TranState->TranArena, TranState->GroundBufferCount, ground_buffer);

        for (uint32 GroundBufferIndex = 0;
            GroundBufferIndex < TranState->GroundBufferCount;
            GroundBufferIndex++) {

            ground_buffer* GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
            GroundBuffer->Bitmap = MakeEmptyBitmap(
                &TranState->TranArena, GroundBufferWidth, GroundBufferHeight, false
            );
            GroundBuffer->P = NullPosition();
        }

        GameState->TestDiffuse = MakeEmptyBitmap(&TranState->TranArena, 256, 256, false);
        DrawRectangle(&GameState->TestDiffuse, V2(0, 0), V2i(GameState->TestDiffuse.Width, GameState->TestDiffuse.Height), V4(0.5f, 0.5f, 0.5f, 1.0f));
        GameState->TestNormal = MakeEmptyBitmap(
            &TranState->TranArena, GameState->TestDiffuse.Width, GameState->TestDiffuse.Height, false
        );
        MakeSphereNormalMap(&GameState->TestNormal, 0.0f);
        MakeSphereDiffuseMap(&GameState->TestDiffuse);

        TranState->EnvMapWidth = 512;
        TranState->EnvMapHeight = 256;
        for (uint32 MapIndex = 0; MapIndex < ArrayCount(TranState->EnvMaps); MapIndex++) {
            environment_map* Map = TranState->EnvMaps + MapIndex;
            uint32 Width = TranState->EnvMapWidth;
            uint32 Height = TranState->EnvMapHeight;
            for (uint32 LODIndex = 0; LODIndex < ArrayCount(Map->LOD); LODIndex++) {
                Map->LOD[LODIndex] = MakeEmptyBitmap(&TranState->TranArena, Width, Height, false);
                Width >>= 1;
                Height >>= 1;
            }
        }

        TranState->IsInitialized = true;
    }

    if (Input->ExecutableReloaded) {
        for (uint32 GroundBufferIndex = 0;
            GroundBufferIndex < TranState->GroundBufferCount;
            GroundBufferIndex++) {

            ground_buffer* GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
            GroundBuffer->P = NullPosition();
        }
    }

    world* World = GameState->World;
    world* TileMap = World;

    for (int32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex) {

        game_controller_input* Controller = GetController(Input, ControllerIndex);

        controlled_hero* ConHero = GameState->ControlledHeroes + ControllerIndex;

        if (ConHero->EntityIndex == 0) {
            if (Controller->Start.EndedDown) {
                *ConHero = {};
                ConHero->EntityIndex = AddPlayer_(GameState).LowIndex;
            }
        } else {

            ConHero->ddP = {};
            ConHero->dSword = {};
            ConHero->dZ = 0.0f;

            if (Controller->IsAnalog) {
                ConHero->ddP = { Controller->StickAverageX, Controller->StickAverageY };
            } else {
                if (Controller->MoveUp.EndedDown) {
                    ConHero->ddP.y = 1;
                }
                if (Controller->MoveDown.EndedDown) {
                    ConHero->ddP.y = -1;
                }
                if (Controller->MoveLeft.EndedDown) {
                    ConHero->ddP.x = -1;
                }
                if (Controller->MoveRight.EndedDown) {
                    ConHero->ddP.x = 1;
                }
            }

            if (Controller->Start.EndedDown) {
                ConHero->dZ = 3.0f;
            }

            if (Controller->ActionUp.EndedDown) {
                ConHero->dSword = { 0.0f, 1.0f };
            }
            if (Controller->ActionDown.EndedDown) {
                ConHero->dSword = { 0.0f, -1.0f };
            }
            if (Controller->ActionLeft.EndedDown) {
                ConHero->dSword = { -1.0f, 0.0f };
            }
            if (Controller->ActionRight.EndedDown) {
                ConHero->dSword = { 1.0f, 0.0f };
            }
        }
    }

    temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);

    render_group* RenderGroup =
        AllocateRenderGroup(&TranState->TranArena, Megabytes(4), GameState->MetersToPixels);

    loaded_bitmap DrawBuffer_ = {};
    loaded_bitmap* DrawBuffer = &DrawBuffer_;
    DrawBuffer->Height = Buffer->Height;
    DrawBuffer->Pitch = Buffer->Pitch;
    DrawBuffer->Width = Buffer->Width;
    DrawBuffer->Memory = Buffer->Memory;

    v2 ScreenCenter = 0.5f * V2((real32)DrawBuffer->Width, (real32)DrawBuffer->Height);

    Clear(RenderGroup, V4(0.25f, 0.25f, 0.25f, 0.0f));

    real32 PixelsToMeters = 1.0f / GameState->MetersToPixels;
    real32 ScreenWidthInMeters = Buffer->Width * PixelsToMeters;
    real32 ScreenHeightInMeters = Buffer->Height * PixelsToMeters;
    rectangle3 CameraBoundsInMeters = RectCenterDim(
        V3(0, 0, 0),
        V3(ScreenWidthInMeters, ScreenHeightInMeters, 0)
    );

    // NOTE(sen) Draw ground
    for (uint32 GroundBufferIndex = 0;
        GroundBufferIndex < TranState->GroundBufferCount;
        GroundBufferIndex++) {

        ground_buffer* GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
        if (IsValid(GroundBuffer->P)) {

            loaded_bitmap* Bitmap = &GroundBuffer->Bitmap;
            v3 Delta = Subtract(GameState->World, &GroundBuffer->P, &GameState->CameraP);

            PushBitmap(RenderGroup, Bitmap, Delta.xy, Delta.z, 0.5f * V2i(Bitmap->Width, Bitmap->Height));
        }
    }

    // NOTE(sen) Fill ground bitmaps
    {
        world_position MinChunkP =
            MapIntoChunkSpace(World, GameState->CameraP, GetMinCorner(CameraBoundsInMeters));
        world_position MaxChunkP =
            MapIntoChunkSpace(World, GameState->CameraP, GetMaxCorner(CameraBoundsInMeters));

        for (int32 ChunkZ = MinChunkP.ChunkZ; ChunkZ <= MaxChunkP.ChunkZ; ChunkZ++) {

            for (int32 ChunkY = MinChunkP.ChunkY; ChunkY <= MaxChunkP.ChunkY; ChunkY++) {

                for (int32 ChunkX = MinChunkP.ChunkX; ChunkX <= MaxChunkP.ChunkX; ChunkX++) {

                    world_chunk* Chunk = GetChunk(World, ChunkX, ChunkY, ChunkZ);
                    world_position ChunkCenterP = CenteredChunkPoint(ChunkX, ChunkY, ChunkZ);
                    v3 RelP = Subtract(GameState->World, &ChunkCenterP, &GameState->CameraP);

                    real32 FurthestBufferLengthSq = 0.0f;
                    ground_buffer* FurthestBuffer = 0;
                    for (uint32 GroundBufferIndex = 0;
                        GroundBufferIndex < TranState->GroundBufferCount;
                        ++GroundBufferIndex) {

                        ground_buffer* GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;

                        if (AreInSameChunk(GameState->World, &GroundBuffer->P, &ChunkCenterP)) {
                            FurthestBuffer = 0;
                            break;
                        } else if (IsValid(GroundBuffer->P)) {
                            v3 RelPBuffer = Subtract(GameState->World, &GroundBuffer->P, &GameState->CameraP);
                            real32 BufferLengthSq = LengthSq(RelPBuffer.xy);
                            if (FurthestBufferLengthSq < BufferLengthSq) {
                                FurthestBufferLengthSq = BufferLengthSq;
                                FurthestBuffer = GroundBuffer;
                            }
                        } else {
                            FurthestBufferLengthSq = Real32Maximum;
                            FurthestBuffer = GroundBuffer;
                            break;
                        }
                    }

                    if (FurthestBuffer) {
                        FillGroundChunk(TranState, GameState, FurthestBuffer, &ChunkCenterP);
                    }
                    PushRectOutline(
                        RenderGroup, RelP.xy, 0.0f, GameState->World->ChunkDimInMeters.xy,
                        V4(1.0f, 1.0f, 0.0f, 1.0f)
                    );
                }
            }
        }
    }

    // NOTE(sen) SimRegion

    v3 SimBoundsExpansion = V3(15.0f, 15.0f, 15.0f);
    rectangle3 SimBounds = AddRadius(CameraBoundsInMeters, SimBoundsExpansion);

    temporary_memory SimMemory = BeginTemporaryMemory(&TranState->TranArena);
    sim_region* SimRegion = BeginSim(
        &TranState->TranArena, GameState, World, GameState->CameraP, SimBounds, Input->dtForFrame
    );

    //* Draw entities
    for (uint32 EntityIndex = 0;
        EntityIndex < SimRegion->EntityCount;
        EntityIndex++) {

        sim_entity* Entity = SimRegion->Entities + EntityIndex;

        if (!Entity->Updatable) {
            continue;
        }

        real32 dt = Input->dtForFrame;

        real32 ShadowAlpha = Maximum(0.0f, 1.0f - 0.5f * Entity->P.z);

        move_spec MoveSpec = DefaultMoveSpec();
        v3 ddP = {};

        render_basis* Basis = PushStruct(&TranState->TranArena, render_basis);
        RenderGroup->DefaultBasis = Basis;

        hero_bitmaps* HeroBitmaps = &GameState->HeroBitmaps[Entity->FacingDirection];
        switch (Entity->Type) {
        case EntityType_Hero:
        {
            for (uint32 ControlIndex = 0;
                ControlIndex < ArrayCount(GameState->ControlledHeroes);
                ++ControlIndex) {

                controlled_hero* ConHero = GameState->ControlledHeroes + ControlIndex;

                if (ConHero->EntityIndex == Entity->StorageIndex) {
                    if (ConHero->dZ != 0.0f) {
                        Entity->dP.z = ConHero->dZ;
                    }

                    MoveSpec.Drag = 8.0f;
                    MoveSpec.Speed = 150.0f;
                    MoveSpec.UnitMaxAccelVector = true;
                    ddP = V3(ConHero->ddP, 0.0f);

                    if (ConHero->dSword.x != 0.0f || ConHero->dSword.y != 0.0f) {

                        sim_entity* Sword = Entity->Sword.Ptr;
                        if (Sword && IsSet(Sword, EntityFlag_Nonspatial)) {
                            Sword->DistanceLimit = 5.0f;
                            MakeEntitySpatial(
                                Sword, Entity->P, Entity->dP + 15.0f * V3(ConHero->dSword, 0.0f)
                            );
                            AddCollisionRule(
                                GameState, Sword->StorageIndex, Entity->StorageIndex, false
                            );
                        }
                    }
                }
            }


            PushBitmap(RenderGroup, &GameState->HeroShadow, { 0, 0 }, 0, HeroBitmaps->Align, ShadowAlpha, 0.0f);
            PushBitmap(RenderGroup, &HeroBitmaps->Head, { 0, 0 }, 0, HeroBitmaps->Align);
            PushBitmap(RenderGroup, &HeroBitmaps->Torso, { 0, 0 }, 0, HeroBitmaps->Align);
            PushBitmap(RenderGroup, &HeroBitmaps->Cape, { 0, 0 }, 0, HeroBitmaps->Align);

            DrawHitpoints_(Entity, RenderGroup);
        }
        break;
        case EntityType_Wall:
        {
            PushBitmap(RenderGroup, &GameState->Tree, { 0, 0 }, 0, { 40, 80 });
        }
        break;
        case EntityType_Stairwell:
        {
            PushRect(RenderGroup, V2(0, 0), 0, Entity->WalkableDim, V4(1, 0.5f, 0, 1), 0.0f);
            PushRect(RenderGroup, V2(0, 0), Entity->WalkableHeight, Entity->WalkableDim, V4(1, 1, 0, 1), 0.0f);
        }
        break;
        case EntityType_Sword:
        {
            MoveSpec.Speed = 0.0f;
            MoveSpec.Drag = 0.0f;
            MoveSpec.UnitMaxAccelVector = false;

            if (Entity->DistanceLimit <= 0.0f) {
                MakeEntityNonSpatial(Entity);
                ClearCollisionRules(GameState, Entity->StorageIndex);
            }
            PushBitmap(RenderGroup, &GameState->HeroShadow, { 0, 0 }, 0, HeroBitmaps->Align, ShadowAlpha, 0.0f);
            PushBitmap(RenderGroup, &GameState->Sword, { 0, 0 }, 0, { 29, 10 });
        }
        break;
        case EntityType_Familiar:
        {
            Entity->tBob += dt;
            real32 Pi2 = 2 * Pi32;
            if (Entity->tBob > Pi2) {
                Entity->tBob -= Pi2;
            }
            real32 BobSin = Sin(4.0f * Entity->tBob);

            sim_entity* ClosestHero = 0;
            real32 ClosestHeroDSq = Square(15.0f);

#if 0
            for (uint32 TestEntityIndex = 0;
                TestEntityIndex < SimRegion->EntityCount;
                TestEntityIndex++) {

                sim_entity* TestEntity = SimRegion->Entities + TestEntityIndex;

                if (TestEntity->Type == EntityType_Hero) {
                    real32 TestDSq = LengthSq(TestEntity->P - Entity->P);
                    if (TestDSq < ClosestHeroDSq) {
                        ClosestHeroDSq = TestDSq;
                        ClosestHero = TestEntity;
                    }
                }
            }

#endif
            if (ClosestHero && ClosestHeroDSq > Square(6.0f)) {
                real32 Acceleration = 0.3f;
                real32 OneOverLength = (Acceleration / SquareRoot(ClosestHeroDSq));
                ddP = (ClosestHero->P - Entity->P) * OneOverLength;
            }
            MoveSpec.Drag = 8.0f;
            MoveSpec.Speed = 150.0f;
            MoveSpec.UnitMaxAccelVector = true;

            PushBitmap(RenderGroup, &GameState->HeroShadow, { 0, 0 }, 0, HeroBitmaps->Align, ShadowAlpha * 0.5f + BobSin * 0.2f);
            PushBitmap(RenderGroup, &HeroBitmaps->Head, { 0, 0 }, 0.2f * BobSin, HeroBitmaps->Align);
        }
        break;
        case EntityType_Monster:
        {
            PushBitmap(RenderGroup, &GameState->HeroShadow, { 0, 0 }, 0, HeroBitmaps->Align, ShadowAlpha);
            PushBitmap(RenderGroup, &HeroBitmaps->Torso, { 0, 0 }, 0, HeroBitmaps->Align);

            DrawHitpoints_(Entity, RenderGroup);
        }
        break;
        case EntityType_Space:
        {
            for (uint32 VolumeIndex = 0; VolumeIndex < Entity->Collision->VolumeCount; ++VolumeIndex) {
                sim_entity_collision_volume* Volume = Entity->Collision->Volumes + VolumeIndex;
                PushRectOutline(RenderGroup, Volume->OffsetP.xy, 0, Volume->Dim.xy, V4(0, 0.5f, 1, 1), 0.0f);
            }
        }
        break;
        default:
        {
            InvalidCodePath;
        }
        break;
        }

        if (!IsSet(Entity, EntityFlag_Nonspatial) && IsSet(Entity, EntityFlag_Moveable)) {
            MoveEntity(GameState, SimRegion, Entity, Input->dtForFrame, &MoveSpec, ddP);
        }

        Basis->P = GetEntityGroundPoint(Entity);
    }

    GameState->Time += Input->dtForFrame * 0.1f;
    real32 Disp = 0.0f;// 130.0f * Cos(10.0f * GameState->Time);

    v2 Origin = ScreenCenter + V2(Disp, Disp);
    real32 Angle = GameState->Time * 15.0f;
    v2 XAxis = 150.0f * V2(Cos(Angle), Sin(Angle)); // 300.0f * V2(Cos(Angle), Sin(Angle));
    v2 YAxis = Perp(XAxis); //100.0f * V2(Cos(Angle + 1.0f), Sin(Angle + 1.0f));

    v4 MapColor[3] = {
        V4(1.0f, 0, 0, 1.0f),
        V4(0, 1.0f, 0, 1.0f),
        V4(0, 0, 1.0f, 1.0f),
    };
#if 1
    for (uint32 MapIndex = 0;
        MapIndex < ArrayCount(TranState->EnvMaps);
        MapIndex++) {

        environment_map* Map = TranState->EnvMaps + MapIndex;
        loaded_bitmap* LOD = Map->LOD + 0;
        uint32 CheckerWidth = 16;
        uint32 CheckerHeight = 16;
        bool32 RowCheckerOn = false;
        for (int32 Y = 0; Y < LOD->Height; Y += CheckerHeight) {
            bool32 CheckerOn = RowCheckerOn;
            for (int32 X = 0; X < LOD->Width; X += CheckerWidth) {
                v2 MinP = V2i(X, Y);
                v2 MaxP = MinP + V2i(CheckerWidth, CheckerHeight);
                v4 Color = MapColor[MapIndex];
                if (CheckerOn) {
                    Color = V4(0, 0, 0, 1.0f);
                }
                CheckerOn = !CheckerOn;
                DrawRectangle(LOD, MinP, MaxP, Color);
            }
            RowCheckerOn = !RowCheckerOn;
        }
    }
#endif

    CoordinateSystem(
        RenderGroup, Origin - 0.5f * XAxis - 0.5f * YAxis, XAxis, YAxis,
        V4(1.0f, 1.0f, 1.0f, 1.0f),
        &GameState->TestDiffuse, &GameState->TestNormal,
        TranState->EnvMaps + 2,
        TranState->EnvMaps + 1,
        TranState->EnvMaps + 0
    );

    v2 MapP = V2(0, 0);
    for (uint32 MapIndex = 0;
        MapIndex < ArrayCount(TranState->EnvMaps);
        MapIndex++) {
        environment_map* Map = TranState->EnvMaps + MapIndex;
        loaded_bitmap* LOD = Map->LOD + 0;
        XAxis = 0.5f * V2i(LOD->Width, 0);
        YAxis = 0.5f * V2i(0, LOD->Height);
        CoordinateSystem(
            RenderGroup,
            MapP,
            XAxis,
            YAxis,
            V4(1.0f, 1.0f, 1.0f, 1.0f),
            LOD,
            0, 0, 0, 0
        );
        MapP += YAxis + V2(0.0f, 6.0f);
    }
    TranState->EnvMaps[0].Pz = -2.0f;
    TranState->EnvMaps[1].Pz = 0.0f;
    TranState->EnvMaps[2].Pz = 2.0f;

#if 0
    Saturation(RenderGroup, 1.0f);
#endif

    RenderGroupToOutput(RenderGroup, DrawBuffer);

    EndSim(SimRegion, GameState);

    EndTemporaryMemory(SimMemory);
    EndTemporaryMemory(RenderMemory);

    CheckArena(&GameState->WorldArena);
    CheckArena(&TranState->TranArena);
}
