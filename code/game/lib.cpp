#include "lib.hpp"
#include "world.cpp"
#include "sim_region.cpp"
#include "asset.cpp"
#include "audio.cpp"
#include "render_group.cpp"
#include "random.cpp"
#include "math.cpp"
#include <math.h>

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
            PushRect(RenderGroup, V3(HitP, 0), HealthDim, Color);
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

    Result.Width = SafeTruncateToUint16(Width);
    Result.Height = SafeTruncateToUint16(Height);
    Result.Pitch = SafeTruncateToUint16(Width * BITMAP_BYTES_PER_PIXEL);

    int32 TotalBitmapSize = Width * Height * BITMAP_BYTES_PER_PIXEL;
    Result.Memory = PushSize(Arena, TotalBitmapSize, 16);

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
            v3 BaseColor = V3(0.0f, 0.0f, 0.0f);
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

internal task_with_memory* BeginTaskWithMemory(transient_state* TranState) {
    task_with_memory* FoundTask = 0;
    for (uint32 TaskIndex = 0; TaskIndex < ArrayCount(TranState->Tasks); ++TaskIndex) {
        task_with_memory* Task = TranState->Tasks + TaskIndex;
        if (!Task->BeingUsed) {
            Task->BeingUsed = true;
            Task->MemoryFlush = BeginTemporaryMemory(&Task->Arena);
            FoundTask = Task;
            break;
        }
    }
    return FoundTask;
}

internal void EndTaskWithMemory(task_with_memory* Task) {
    EndTemporaryMemory(Task->MemoryFlush);
    CompletePreviousWritesBeforeFutureWrites;
    Task->BeingUsed = false;
}

struct load_asset_work {
    task_with_memory* Task;
    asset_slot* Slot;
    platform_file_handle* Handle;
    uint64 Offset;
    uint64 Size;
    void* Destination;
    asset_state FinalState;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork) {
    load_asset_work* Work = (load_asset_work*)Data;

    Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);

    CompletePreviousWritesBeforeFutureWrites;

    if (!PlatformNoFileErrors(Work->Handle)) {
        ZeroSize(Work->Size, Work->Destination);
    }

    Work->Slot->State = Work->FinalState;

    EndTaskWithMemory(Work->Task);
}

internal inline platform_file_handle*
GetFileHandleFor(game_assets* Assets, uint32 FileIndex) {
    Assert(FileIndex < Assets->FileCount);
    platform_file_handle* Handle = Assets->Files[FileIndex].Handle;
    return Handle;
}

internal void* AcquireAssetMemory(game_assets* Assets, memory_index Size) {
    void* Result = Platform.AllocateMemory(Size);
    if (Result) {
        Assets->TotalMemoryUsed += Size;
    }
    return Result;
}

internal void ReleaseAssetMemory(game_assets* Assets, memory_index Size, void* Memory) {
    Platform.DeallocateMemory(Memory);
    if (Memory) {
        Assets->TotalMemoryUsed -= Size;
    }
}

internal void LoadBitmap(game_assets* Assets, bitmap_id ID) {
    asset_slot* Slot = Assets->Slots + ID.Value;
    if (ID.Value && AtomicCompareExchangeUint32((uint32*)&Slot->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded) {
        task_with_memory* Task = BeginTaskWithMemory(Assets->TranState);
        if (Task) {

            asset* Asset = Assets->Assets + ID.Value;
            hha_asset* HHAAsset = &Asset->HHA;
            hha_bitmap* Info = &HHAAsset->Bitmap;

            loaded_bitmap* Bitmap = &Slot->Bitmap;

            Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
            Bitmap->Width = SafeTruncateToUint16(Info->Dim[0]);
            Bitmap->Height = SafeTruncateToUint16(Info->Dim[1]);
            Bitmap->Pitch = SafeTruncateToUint16(Bitmap->Width * 4);
            Bitmap->WidthOverHeight = (real32)Bitmap->Width / (real32)Bitmap->Height;

            uint32 MemorySize = Bitmap->Pitch * Bitmap->Height;
            Bitmap->Memory = AcquireAssetMemory(Assets, MemorySize); //PushSize(&Assets->Arena, MemorySize);

            load_asset_work* Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Slot = Assets->Slots + ID.Value;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Offset = HHAAsset->DataOffset;
            Work->Size = MemorySize;
            Work->Destination = Bitmap->Memory;
            Work->FinalState = AssetState_Loaded;

            //Copy(MemorySize, Assets->HHAContents + HHAAsset->DataOffset, Bitmap->Memory);
#if 1
            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
#else
            LoadAssetWork(Assets->TranState->LowPriorityQueue, Work);
#endif
        } else {
            Slot->State = AssetState_Unloaded;
        }
    }
}

internal void LoadSound(game_assets* Assets, sound_id ID) {
    asset_slot* Slot = Assets->Slots + ID.Value;
    if (ID.Value && AtomicCompareExchangeUint32((uint32*)&Slot->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded) {
        task_with_memory* Task = BeginTaskWithMemory(Assets->TranState);
        if (Task) {
            asset* Asset = Assets->Assets + ID.Value;
            hha_asset* HHAAsset = &Asset->HHA;
            hha_sound* Info = &HHAAsset->Sound;
            loaded_sound* Sound = &Slot->Sound;
            Sound->SampleCount = Info->SampleCount;
            Sound->ChannelCount = Info->ChannelCount;
            uint32 MemorySize = Sound->ChannelCount * Sound->SampleCount * sizeof(int16);
            void* Memory = AcquireAssetMemory(Assets, MemorySize); // PushSize(&Assets->Arena, MemorySize);

            int16* SoundAt = (int16*)Memory;
            for (uint32 ChannelIndex = 0; ChannelIndex < Sound->ChannelCount; ++ChannelIndex) {
                Sound->Samples[ChannelIndex] = SoundAt;
                SoundAt += Sound->SampleCount;
            }

            load_asset_work* Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Slot = Assets->Slots + ID.Value;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Offset = HHAAsset->DataOffset;
            Work->Size = MemorySize;
            Work->Destination = Memory;
            Work->FinalState = AssetState_Loaded;

            //Copy(MemorySize, Assets->HHAContents + HHAAsset->DataOffset, Memory);
#if 1
            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
#else
            LoadSoundWork(Assets->TranState->LowPriorityQueue, Work);
#endif
        } else {
            Slot->State = AssetState_Unloaded;
        }
    }
}

internal void PrefetchSound(game_assets* Assets, sound_id ID) {
    LoadSound(Assets, ID);
}

internal void PrefetchBitmap(game_assets* Assets, bitmap_id ID) {
    LoadBitmap(Assets, ID);
}

struct fill_ground_chunk_work {
    render_group* RenderGroup;
    loaded_bitmap* Buffer;
    task_with_memory* Task;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(FillGroundChunkWork) {
    fill_ground_chunk_work* Work = (fill_ground_chunk_work*)Data;
    RenderGroupToOutput(Work->RenderGroup, Work->Buffer);
    EndTaskWithMemory(Work->Task);
}

internal void FillGroundChunk(
    transient_state* TranState, game_state* GameState,
    ground_buffer* GroundBuffer, world_position* ChunkP
) {
    task_with_memory* Task = BeginTaskWithMemory(TranState);
    if (Task) {
        fill_ground_chunk_work* Work = PushStruct(&Task->Arena, fill_ground_chunk_work);

        loaded_bitmap* Buffer = &GroundBuffer->Bitmap;
        Buffer->AlignPercentage = V2(0.5f, 0.5f);
        Buffer->WidthOverHeight = 1.0f;

        real32 Width = GameState->World->ChunkDimInMeters.x;
        real32 Height = GameState->World->ChunkDimInMeters.y;
        Assert(Width == Height);
        v2 HalfDim = 0.5f * V2(Width, Height);

        render_group* RenderGroup = AllocateRenderGroup(TranState->Assets, &Task->Arena, 0);
        Orthographic(RenderGroup, Buffer->Width, Buffer->Height, (real32)(Buffer->Width - 2) / Width);
        Clear(RenderGroup, V4(1.0f, 0.5f, 0.0f, 1.0f));

        Work->Buffer = Buffer;
        Work->RenderGroup = RenderGroup;
        Work->Task = Task;

        for (int32 ChunkOffsetY = -1; ChunkOffsetY <= 1; ChunkOffsetY++) {
            for (int32 ChunkOffsetX = -1; ChunkOffsetX <= 1; ChunkOffsetX++) {

                int32 ChunkX = ChunkP->ChunkX + ChunkOffsetX;
                int32 ChunkY = ChunkP->ChunkY + ChunkOffsetY;
                int32 ChunkZ = ChunkP->ChunkZ;

                random_series Series = RandomSeed(139 * ChunkX + 593 * ChunkY + 329 * ChunkZ);

#if 0
                v4 Color = V4(1.0f, 0.0f, 0.0f, 1.0f);
                if (ChunkX % 2 == ChunkY % 2) {
                    Color = V4(0.0f, 0.0f, 1.0f, 1.0f);
                }
#else
                v4 Color = V4(1, 1, 1, 1);
#endif

                v2 Center = V2(ChunkOffsetX * Width, ChunkOffsetY * Height);

                for (uint32 GrassIndex = 0; GrassIndex < 100; ++GrassIndex) {

                    bitmap_id Stamp = GetRandomBitmapFrom(TranState->Assets, RandomChoice(&Series, 2) ? Asset_Grass : Asset_Stone, &Series);;

                    v2 Offset =
                        Hadamard(HalfDim, V2(RandomBilateral(&Series), RandomBilateral(&Series)));
                    v2 P = Center + Offset;
                    PushBitmap(RenderGroup, Stamp, 2.0f, V3(P, 0), Color);
                }
            }
        }

        for (int32 ChunkOffsetY = -1; ChunkOffsetY <= 1; ChunkOffsetY++) {
            for (int32 ChunkOffsetX = -1; ChunkOffsetX <= 1; ChunkOffsetX++) {

                int32 ChunkX = ChunkP->ChunkX + ChunkOffsetX;
                int32 ChunkY = ChunkP->ChunkY + ChunkOffsetY;
                int32 ChunkZ = ChunkP->ChunkZ;

                random_series Series = RandomSeed(139 * ChunkX + 593 * ChunkY + 329 * ChunkZ);

                v2 Center = V2(ChunkOffsetX * Width, ChunkOffsetY * Height);

                for (uint32 GrassIndex = 0; GrassIndex < 50; ++GrassIndex) {
                    bitmap_id Stamp = GetRandomBitmapFrom(TranState->Assets, Asset_Tuft, &Series);

                    v2 Offset =
                        Hadamard(HalfDim, V2(RandomBilateral(&Series), RandomBilateral(&Series)));
                    v2 P = Center + Offset;
                    PushBitmap(RenderGroup, Stamp, 0.1f, V3(P, 0));
                }
            }
        }

        if (AllResourcesPresent(RenderGroup)) {
            GroundBuffer->P = *ChunkP;
            Platform.AddEntry(TranState->LowPriorityQueue, FillGroundChunkWork, Work);
        } else {
            EndTaskWithMemory(Task);
        }
    }
}

struct hero_bitmap_ids {
    bitmap_id Head;
    bitmap_id Cape;
    bitmap_id Torso;
};

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
    Platform = Memory->PlatformAPI;
#if HANDMADE_INTERNAL
    DebugGlobalMemory = Memory;
#endif
    BEGIN_TIMED_BLOCK(GameUpdateAndRender);
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    Assert(
        &Input->Controllers[0].Terminator - &Input->Controllers[0].MoveUp ==
        ArrayCount(Input->Controllers[0].Buttons)
    );

    game_state* GameState = (game_state*)Memory->PermanentStorage;

    uint32 GroundBufferWidth = 256;
    uint32 GroundBufferHeight = 256;

    if (!GameState->IsInitialized) {

        InitializeArena(
            &GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
            (uint8*)Memory->PermanentStorage + sizeof(game_state)
        );

        InitializeAudioState(&GameState->AudioState, &GameState->WorldArena);

        GameState->EffectsEntropy = RandomSeed(1234);

        AddLowEntity_(GameState, EntityType_Null, NullPosition());

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world* TileMap = GameState->World;


        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;

        GameState->TypicalFloorHeight = 3.0f;

        real32 PixelsToMeters = 1.0f / 42.0f;
        v3 WorldChunkDimInMeters = V3(
            PixelsToMeters * (real32)GroundBufferWidth,
            PixelsToMeters * (real32)GroundBufferHeight,
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

            uint32 DoorDirection = RandomChoice(&Series, DoorUp || DoorDown ? 2 : 4);
            //DoorDirection = 3;

            bool32 CreatedZDoor = false;

            if (DoorDirection == 3) {
                CreatedZDoor = true;
                DoorDown = true;
            } else if (DoorDirection == 2) {
                CreatedZDoor = true;
                DoorUp = true;
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
                    if ((TileX == 0) && (!DoorLeft || TileY != TilesPerHeight / 2)) {
                        ShouldBeDoor = true;
                    }
                    if ((TileX == TilesPerWidth - 1) && (!DoorRight || TileY != TilesPerHeight / 2)) {
                        ShouldBeDoor = true;
                    }
                    if ((TileY == 0) && (!DoorBottom || TileX != TilesPerWidth / 2)) {
                        ShouldBeDoor = true;
                    }
                    if ((TileY == TilesPerHeight - 1) && (!DoorTop || TileX != TilesPerWidth / 2)) {
                        ShouldBeDoor = true;
                    }

                    if (ShouldBeDoor) {
                        AddWall_(GameState, AbsTileX, AbsTileY, AbsTileZ);

                    } else if (CreatedZDoor) {
                        if (AbsTileZ % 2 == 1 && (TileX == 10 && TileY == 5)) {
                            AddStair(
                                GameState, AbsTileX, AbsTileY, DoorDown ? AbsTileZ - 1 : AbsTileZ
                            );
                        } else if ((AbsTileZ % 2 == 0 && (TileX == 7 && TileY == 5))) {
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

            if (DoorDirection == 3) {
                AbsTileZ -= 1;
            } else if (DoorDirection == 2) {
                AbsTileZ += 1;
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


        GameState->IsInitialized = true;
    }

    Assert(sizeof(transient_state) <= Memory->TransientStorageSize);

    transient_state* TranState = (transient_state*)Memory->TransientStorage;
    if (!TranState->IsInitialized) {
        InitializeArena(
            &TranState->TranArena, Memory->TransientStorageSize - sizeof(transient_state),
            (uint8*)Memory->TransientStorage + sizeof(transient_state)
        );

        TranState->HighPriorityQueue = Memory->HighPriorityQueue;
        TranState->LowPriorityQueue = Memory->LowPriorityQueue;

        for (uint32 TaskIndex = 0; TaskIndex < ArrayCount(TranState->Tasks); TaskIndex++) {
            task_with_memory* Task = TranState->Tasks + TaskIndex;
            Task->BeingUsed = false;
            SubArena(&Task->Arena, &TranState->TranArena, Megabytes(1));
        }

        TranState->Assets = AllocateGameAssets(&TranState->TranArena, Megabytes(3), TranState);

        GameState->Music = PlaySound(&GameState->AudioState, GetFirstSoundFrom(TranState->Assets, Asset_Music));
        ChangeVolume(&GameState->AudioState, GameState->Music, 0.1f, V2(0.1f, 0.1f));

        TranState->GroundBufferCount = 128;
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

    // NOTE(sen) Debug sound volume/pitch shift
#if 0
    {
        v2 MusicVolume;
        MusicVolume.E[1] = Clamp01((real32)Input->MouseX / (real32)Buffer->Width);
        MusicVolume.E[0] = 1.0f - MusicVolume.E[1];
        ChangeVolume(&GameState->AudioState, GameState->Music, 0.1f, MusicVolume);

        real32 MusicPitch = Clamp01((real32)Input->MouseY / (real32)Buffer->Height);
        ChangePitch(&GameState->AudioState, GameState->Music, MusicPitch + 0.5f);
    }
#endif

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
#if 0
            if (Controller->Start.EndedDown) {
                ConHero->dZ = 3.0f;
            }
#endif
#if 0
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
#else
            real32 ZoomRate = 0.0f;
            if (Controller->ActionUp.EndedDown) {
                ChangePitch(&GameState->AudioState, GameState->Music, 0.5f);
                ZoomRate = 1.0f;
            }
            if (Controller->ActionDown.EndedDown) {
                ChangePitch(&GameState->AudioState, GameState->Music, 1.0f);
                ZoomRate = -1.0f;
            }
            if (Controller->ActionLeft.EndedDown) {
                ChangeVolume(&GameState->AudioState, GameState->Music, 1.0f, V2(0, 0));
                ConHero->dSword = { -1.0f, 0.0f };
            }
            if (Controller->ActionRight.EndedDown) {
                ChangeVolume(&GameState->AudioState, GameState->Music, 1.0f, V2(1.0f, 1.0f));
                ConHero->dSword = { 1.0f, 0.0f };
            }
#endif
        }
    }

    temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);

    loaded_bitmap DrawBuffer_ = {};
    loaded_bitmap* DrawBuffer = &DrawBuffer_;
    DrawBuffer->Height = SafeTruncateToUint16(Buffer->Height);
    DrawBuffer->Pitch = SafeTruncateToUint16(Buffer->Pitch);
    DrawBuffer->Width = SafeTruncateToUint16(Buffer->Width);
    DrawBuffer->Memory = Buffer->Memory;

    render_group* RenderGroup = AllocateRenderGroup(TranState->Assets, &TranState->TranArena, Megabytes(4));

    real32 WidthOfMonitor = 0.635f;
    real32 MetersToPixels = (real32)(DrawBuffer->Width) * WidthOfMonitor; // NOTE(sen) Should be a division;
    real32 FocalLength = 0.6f;
    real32 DistanceAboveGround = 9.0f;
    Perspective(
        RenderGroup, DrawBuffer->Width, DrawBuffer->Height, MetersToPixels,
        FocalLength, DistanceAboveGround
    );

    v2 ScreenCenter = 0.5f * V2((real32)DrawBuffer->Width, (real32)DrawBuffer->Height);

    Clear(RenderGroup, V4(0.25f, 0.25f, 0.25f, 0.0f));

    rectangle2 ScreenBounds = GetCameraRectagleAtTarget(RenderGroup);
    rectangle3 CameraBoundsInMeters =
        RectMinMax(V3(ScreenBounds.Min, 0.0f), V3(ScreenBounds.Max, 0.0f));
    CameraBoundsInMeters.Min.z = -3.0f * GameState->TypicalFloorHeight;
    CameraBoundsInMeters.Max.z = 1.0f * GameState->TypicalFloorHeight;

    // NOTE(sen) Draw ground
#if 1
    for (uint32 GroundBufferIndex = 0;
        GroundBufferIndex < TranState->GroundBufferCount;
        GroundBufferIndex++) {

        ground_buffer* GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
        if (IsValid(GroundBuffer->P)) {

            loaded_bitmap* Bitmap = &GroundBuffer->Bitmap;
            v3 Delta = Subtract(GameState->World, &GroundBuffer->P, &GameState->CameraP);
            if (Delta.z >= -1.0f && Delta.z < 1.0f) {
                real32 GroundSideInMeters = GameState->World->ChunkDimInMeters.x;
                PushBitmap(RenderGroup, Bitmap, GroundSideInMeters, Delta);
#if 0
                PushRectOutline(
                    RenderGroup, Delta, V2(GroundSideInMeters, GroundSideInMeters),
                    V4(1.0f, 0.0f, 0.0f, 1.0f)
                );
#endif
            }
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
                }
            }
        }
    }
#endif
    // NOTE(sen) SimRegion
    v3 SimBoundsExpansion = V3(15.0f, 15.0f, 0.0f);
    rectangle3 SimBounds = AddRadius(CameraBoundsInMeters, SimBoundsExpansion);

    temporary_memory SimMemory = BeginTemporaryMemory(&TranState->TranArena);
    world_position SimCenterP = GameState->CameraP;
    sim_region* SimRegion = BeginSim(
        &TranState->TranArena, GameState, World, SimCenterP, SimBounds, Input->dtForFrame
    );

    PushRectOutline(RenderGroup, V3(0, 0, 0), GetDim(ScreenBounds), V4(1.0f, 1.0f, 0.0f, 1.0f));
    PushRectOutline(RenderGroup, V3(0, 0, 0), GetDim(SimBounds).xy, V4(0.0f, 1.0f, 1.0f, 1.0f));
    PushRectOutline(RenderGroup, V3(0, 0, 0), GetDim(SimRegion->UpdatableBounds).xy, V4(1.0f, 1.0f, 1.0f, 1.0f));
    PushRectOutline(RenderGroup, V3(0, 0, 0), GetDim(SimRegion->Bounds).xy, V4(1.0f, 0.0f, 1.0f, 1.0f));

    v3 CameraP = Subtract(GameState->World, &GameState->CameraP, &SimCenterP);

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

        v3 CameraRelativeGroundP = GetEntityGroundPoint(Entity) - CameraP;
        real32 FadeTopEndZ = 0.75f * GameState->TypicalFloorHeight;
        real32 FadeTopStartZ = 0.5f * GameState->TypicalFloorHeight;
        real32 FadeBottomStartZ = -2.0f * GameState->TypicalFloorHeight;
        real32 FadeBottomEndZ = -2.25f * GameState->TypicalFloorHeight;

        RenderGroup->GlobalAlpha = 1.0f;
        if (CameraRelativeGroundP.z > FadeTopStartZ) {
            RenderGroup->GlobalAlpha = Clamp01MapToRange(FadeTopEndZ, CameraRelativeGroundP.z, FadeTopStartZ);
        } else if (CameraRelativeGroundP.z < FadeBottomStartZ) {
            RenderGroup->GlobalAlpha = Clamp01MapToRange(FadeBottomEndZ, CameraRelativeGroundP.z, FadeBottomStartZ);
        }

        hero_bitmap_ids HeroBitmaps = {};
        asset_vector MatchVector = {};
        MatchVector.E[Tag_FacingDirection] = Entity->FacingDirection;
        asset_vector WeightVector = {};
        WeightVector.E[Tag_FacingDirection] = 1.0f;
        HeroBitmaps.Head = GetBestMatchBitmapFrom(TranState->Assets, Asset_Head, &MatchVector, &WeightVector);
        HeroBitmaps.Cape = GetBestMatchBitmapFrom(TranState->Assets, Asset_Cape, &MatchVector, &WeightVector);
        HeroBitmaps.Torso = GetBestMatchBitmapFrom(TranState->Assets, Asset_Torso, &MatchVector, &WeightVector);
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
                            PlaySound(&GameState->AudioState, GetRandomSoundFrom(TranState->Assets, Asset_Bloop, &GameState->EffectsEntropy));
                        }
                    }
                }
            }
        }
        break;
        case EntityType_Wall:
        {
        }
        break;
        case EntityType_Stairwell:
        {
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
        }
        break;
        case EntityType_Familiar:
        {
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
        }
        break;
        case EntityType_Monster:
        {

        }
        break;
        case EntityType_Space:
        {

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

        RenderGroup->Transform.OffsetP = GetEntityGroundPoint(Entity);

        // Post-physics
        switch (Entity->Type) {
        case EntityType_Hero:
        {
            real32 HeroSize = 2.5f;
            PushBitmap(
                RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), HeroSize, V3(0, 0, 0), V4(1, 1, 1, ShadowAlpha)
            );
            PushBitmap(RenderGroup, HeroBitmaps.Head, HeroSize, V3(0, 0, 0));
            PushBitmap(RenderGroup, HeroBitmaps.Torso, HeroSize, V3(0, 0, 0));
            PushBitmap(RenderGroup, HeroBitmaps.Cape, HeroSize, V3(0, 0, 0));
            DrawHitpoints_(Entity, RenderGroup);
        }
        break;
        case EntityType_Wall:
        {
            PushBitmap(RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Tree), 2.5f, V3(0, 0, 0));
        }
        break;
        case EntityType_Stairwell:
        {
            PushRect(RenderGroup, V3(0, 0, 0), Entity->WalkableDim, V4(1, 0.0f, 0, 1));
            PushRect(RenderGroup, V3(0, 0, Entity->WalkableHeight), Entity->WalkableDim, V4(1, 1, 0, 1));
        }
        break;
        case EntityType_Sword:
        {
            PushBitmap(
                RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), 0.5f, V3(0, 0, 0),
                V4(1, 1, 1, ShadowAlpha)
            );
            PushBitmap(RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Sword), 0.5f, V3(0, 0, 0));
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
            PushBitmap(
                RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), 2.5f, V3(0, 0, 0),
                V4(1, 1, 1, ShadowAlpha * 0.5f + BobSin * 0.2f)
            );
            PushBitmap(RenderGroup, HeroBitmaps.Head, 2.5f, V3(0, 0, 0.2f * BobSin));
        }
        break;
        case EntityType_Monster:
        {
            PushBitmap(
                RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), 2.5f, V3(0, 0, 0),
                V4(1, 1, 1, ShadowAlpha)
            );
            PushBitmap(RenderGroup, HeroBitmaps.Torso, 2.5f, V3(0, 0, 0));

            DrawHitpoints_(Entity, RenderGroup);
        }
        break;
        case EntityType_Space:
        {
            for (uint32 VolumeIndex = 0; VolumeIndex < Entity->Collision->VolumeCount; ++VolumeIndex) {
                sim_entity_collision_volume* Volume = Entity->Collision->Volumes + VolumeIndex;
                PushRectOutline(RenderGroup, Volume->OffsetP - V3(0, 0, 0.5f * Volume->Dim.z), Volume->Dim.xy, V4(0, 0.5f, 1, 1));
            }
        }
        break;
        default:
        {
            InvalidCodePath;
        }
        break;
        }
    }

    RenderGroup->GlobalAlpha = 1.0f;

#if 0
    GameState->Time += Input->dtForFrame * 0.1f;
    real32 Disp = 130.0f * Cos(10.0f * GameState->Time);

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

#endif

#if 0
    Saturation(RenderGroup, 1.0f);
#endif

#if 0
    RenderGroup->GlobalAlpha = 1.0f;
    RenderGroup->Transform.OffsetP = V3(0, 0, 0);

    for (uint32 ParticleSpawnIndex = 0; ParticleSpawnIndex < 3; ++ParticleSpawnIndex) {
        particle* Particle = GameState->Particles + GameState->NextParticle++;
        if (GameState->NextParticle >= ArrayCount(GameState->Particles)) {
            GameState->NextParticle = 0;
        }
        Particle->P = V3(RandomBetween(&GameState->EffectsEntropy, -0.05f, 0.05f), 0, 0);
        Particle->dP = V3(
            RandomBetween(&GameState->EffectsEntropy, -0.01f, 0.01f),
            7.0f * RandomBetween(&GameState->EffectsEntropy, 0.7f, 1.0f),
            0.0f
        );
        Particle->ddP = V3(0, -9.8f, 0);
        Particle->Color = V4(
            RandomBetween(&GameState->EffectsEntropy, 0.0f, 1.0f),
            RandomBetween(&GameState->EffectsEntropy, 0.0f, 1.0f),
            RandomBetween(&GameState->EffectsEntropy, 0.0f, 1.0f),
            1.0f
        );
        Particle->dColor = V4(0.0f, 0.0f, 0.0f, -0.25f);
        Particle->BitmapID = GetRandomBitmapFrom(TranState->Assets, Asset_Head, &GameState->EffectsEntropy);
    }

    ZeroStruct(GameState->ParticleCells);

    real32 GridScale = 0.25f;
    real32 InvGridScale = 1.0f / GridScale;
    v3 GridOrigin = GridScale * V3(-0.5f * PARTICLE_CELL_DIM, 0, 0);
    for (uint32 ParticleIndex = 0; ParticleIndex < ArrayCount(GameState->Particles); ++ParticleIndex) {
        particle* Particle = GameState->Particles + ParticleIndex;

        v3 P = InvGridScale * (Particle->P - GridOrigin);

        int32 X = TruncateReal32ToInt32(P.x);
        int32 Y = TruncateReal32ToInt32(P.y);

        if (X < 0) { X = 0; }
        if (X > PARTICLE_CELL_DIM - 1) { X = PARTICLE_CELL_DIM - 1; }
        if (Y < 0) { Y = 0; }
        if (Y > PARTICLE_CELL_DIM) { Y = PARTICLE_CELL_DIM - 1; }

        particle_cell* Cell = &GameState->ParticleCells[Y][X];
        real32 Density = Particle->Color.a;
        Cell->Density += Density;
        Cell->VelocityTimesDensity += Particle->dP * Density;
    }

    for (uint32 Y = 0; Y < PARTICLE_CELL_DIM; ++Y) {
        for (uint32 X = 0; X < PARTICLE_CELL_DIM; ++X) {
            particle_cell* Cell = &GameState->ParticleCells[Y][X];
            real32 Alpha = Clamp01(Cell->Density * 0.1f);
            PushRect(RenderGroup, V3i(X, Y, 0) * GridScale + GridOrigin, GridScale * V2(1.0f, 1.0f), V4(Alpha, Alpha, Alpha, Alpha));
        }
    }

    for (uint32 ParticleIndex = 0; ParticleIndex < ArrayCount(GameState->Particles); ++ParticleIndex) {
        particle* Particle = GameState->Particles + ParticleIndex;

        v3 P = InvGridScale * (Particle->P - GridOrigin);

        int32 X = TruncateReal32ToInt32(P.x);
        int32 Y = TruncateReal32ToInt32(P.y);

        if (X < 1) { X = 1; }
        if (X > PARTICLE_CELL_DIM - 2) { X = PARTICLE_CELL_DIM - 2; }
        if (Y < 1) { Y = 1; }
        if (Y > PARTICLE_CELL_DIM) { Y = PARTICLE_CELL_DIM - 2; }

        particle_cell* CellCenter = &GameState->ParticleCells[Y][X];
        particle_cell* CellLeft = &GameState->ParticleCells[Y][X - 1];
        particle_cell* CellRight = &GameState->ParticleCells[Y][X + 1];
        particle_cell* CellDown = &GameState->ParticleCells[Y - 1][X];
        particle_cell* CellUp = &GameState->ParticleCells[Y + 1][X];

        v3 Dispersion = {};
        real32 Dc = 1.0f;
        Dispersion += Dc * (CellCenter->Density - CellLeft->Density) * V3(-1.0f, 0, 0);
        Dispersion += Dc * (CellCenter->Density - CellRight->Density) * V3(1.0f, 0, 0);
        Dispersion += Dc * (CellCenter->Density - CellDown->Density) * V3(0.0f, -1.0f, 0);
        Dispersion += Dc * (CellCenter->Density - CellUp->Density) * V3(0.0f, 1.0f, 0);

        v3 ddP = Particle->ddP + Dispersion;

        Particle->P += 0.5f * ddP * Square(Input->dtForFrame) + Particle->dP * Input->dtForFrame;
        Particle->dP += ddP * Input->dtForFrame;
        Particle->Color += Input->dtForFrame * Particle->dColor;

        if (Particle->P.y < 0) {
            real32 Restitution = 0.5f;
            real32 Friction = 0.7f;
            Particle->P.y = -Particle->P.y;
            Particle->dP.y = -Particle->dP.y * Restitution;
            Particle->dP.x *= Friction;
        }

        v4 Color = Clamp01(Particle->Color);

        if (Color.a > 0.9f) {
            Color.a = 0.9f * Clamp01MapToRange(1.0f, Color.a, 0.9f);
        }

        PushBitmap(RenderGroup, Particle->BitmapID, 1.0f, Particle->P, Color);
    }
#endif

    TiledRenderGroupToOutput(TranState->HighPriorityQueue, RenderGroup, DrawBuffer);

    EndSim(SimRegion, GameState);

    EndTemporaryMemory(SimMemory);
    EndTemporaryMemory(RenderMemory);

    CheckArena(&GameState->WorldArena);
    CheckArena(&TranState->TranArena);

    END_TIMED_BLOCK(GameUpdateAndRender);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    transient_state* TranState = (transient_state*)Memory->TransientStorage;
    OutputPlayingSounds(
        &GameState->AudioState, SoundBuffer, TranState->Assets, &TranState->TranArena
    );
}
