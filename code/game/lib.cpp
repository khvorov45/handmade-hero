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

    Result.Width = Width;
    Result.Height = Height;
    Result.Pitch = Width * BITMAP_BYTES_PER_PIXEL;
    Result.AlignPercentage = V2(0.5f, 0.5f);
    Result.WidthOverHeight = SafeRatio1((real32)Result.Width, (real32)Result.Height);

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

enum finalize_asset_operation {
    FinalizeAsset_None,
    FinalizeAsset_Font,
};

struct load_asset_work {
    task_with_memory* Task;
    asset* Asset;
    platform_file_handle* Handle;
    uint64 Offset;
    uint64 Size;
    void* Destination;
    finalize_asset_operation FinalizeOperation;
    uint32 FinalState;
};

internal void
LoadAssetWorkDirectly(load_asset_work* Work) {
    Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);
    if (PlatformNoFileErrors(Work->Handle)) {
        switch (Work->FinalizeOperation) {
        case FinalizeAsset_None: {} break;
        case FinalizeAsset_Font: {
            loaded_font* Font = &Work->Asset->Header->Font;
            hha_font* HHA = &Work->Asset->HHA.Font;
            for (uint32 GlyphIndex = 1; GlyphIndex < HHA->GlyphCount; ++GlyphIndex) {
                hha_font_glyph* Glyph = Font->Glyphs + GlyphIndex;
                Assert(Glyph->UnicodeCodepoint < HHA->OnePastHighestCodepoint);
                Assert((uint32)(uint16)GlyphIndex == GlyphIndex);
                Font->UnicodeMap[Glyph->UnicodeCodepoint] = (uint16)GlyphIndex;
            }
        } break;
        }
    }
    CompletePreviousWritesBeforeFutureWrites;
    if (!PlatformNoFileErrors(Work->Handle)) {
        ZeroSize(Work->Size, Work->Destination);
    }
    Work->Asset->State = Work->FinalState;
}

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork) {
    load_asset_work* Work = (load_asset_work*)Data;
    LoadAssetWorkDirectly(Work);
    EndTaskWithMemory(Work->Task);
}

internal inline asset_file*
GetFile(game_assets* Assets, uint32 FileIndex) {
    Assert(FileIndex < Assets->FileCount);
    asset_file* Result = Assets->Files + FileIndex;
    return Result;
}

internal inline platform_file_handle*
GetFileHandleFor(game_assets* Assets, uint32 FileIndex) {
    platform_file_handle* Handle = &GetFile(Assets, FileIndex)->Handle;
    return Handle;
}

internal void LoadBitmap(game_assets* Assets, bitmap_id ID, bool32 Immediate) {
    asset* Asset = Assets->Assets + ID.Value;
    if (ID.Value) {
        if (AtomicCompareExchangeUint32((uint32*)&Asset->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded) {
            task_with_memory* Task = 0;
            if (!Immediate) {
                Task = BeginTaskWithMemory(Assets->TranState);
            }
            if (Immediate || Task) {

                hha_asset* HHAAsset = &Asset->HHA;
                hha_bitmap* Info = &HHAAsset->Bitmap;

                asset_memory_size Size = {};
                uint32 Width = Info->Dim[0];
                uint32 Height = Info->Dim[1];
                Size.Section = Width * 4;
                Size.Data = Size.Section * Height;
                Size.Total = Size.Data + sizeof(asset_memory_header);

                Asset->Header = (asset_memory_header*)AcquireAssetMemory(Assets, Size.Total, ID.Value);

                loaded_bitmap* Bitmap = &Asset->Header->Bitmap;

                Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
                Bitmap->Width = Info->Dim[0];
                Bitmap->Height = Info->Dim[1];
                Bitmap->WidthOverHeight = (real32)Bitmap->Width / (real32)Bitmap->Height;
                Bitmap->Pitch = Size.Section;
                Bitmap->Memory = Asset->Header + 1;

                load_asset_work Work;
                Work.Task = Task;
                Work.Asset = Assets->Assets + ID.Value;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Offset = HHAAsset->DataOffset;
                Work.Size = Size.Data;
                Work.Destination = Bitmap->Memory;
                Work.FinalizeOperation = FinalizeAsset_None;
                Work.FinalState = AssetState_Loaded;
                if (Task) {
                    load_asset_work* TaskWork = PushStruct(&Task->Arena, load_asset_work);
                    *TaskWork = Work;
                    Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, TaskWork);
                } else {
                    LoadAssetWorkDirectly(&Work);
                }
            } else {
                Asset->State = AssetState_Unloaded;
            }
        } else if (Immediate) {
            asset_state volatile* State = (asset_state volatile*)&Asset->State;
            while (*State == AssetState_Queued) {}
        }
    }
}

internal void LoadFont(game_assets* Assets, font_id ID, bool32 Immediate) {
    asset* Asset = Assets->Assets + ID.Value;
    if (ID.Value) {
        if (AtomicCompareExchangeUint32((uint32*)&Asset->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded) {
            task_with_memory* Task = 0;
            if (!Immediate) {
                Task = BeginTaskWithMemory(Assets->TranState);
            }
            if (Immediate || Task) {

                hha_asset* HHAAsset = &Asset->HHA;
                hha_font* Info = &HHAAsset->Font;

                uint32 GlyphsSize = sizeof(hha_font_glyph) * Info->GlyphCount;
                uint32 HorizontalAdvanceSize = sizeof(real32) * Info->GlyphCount * Info->GlyphCount;
                uint32 SizeData = GlyphsSize + HorizontalAdvanceSize;
                uint32 UnicodeMapSize = sizeof(uint16) * Info->OnePastHighestCodepoint;
                uint32 SizeTotal = SizeData + sizeof(asset_memory_header) + UnicodeMapSize;

                Asset->Header = (asset_memory_header*)AcquireAssetMemory(Assets, SizeTotal, ID.Value);

                loaded_font* Font = &Asset->Header->Font;
                Font->BitmapIDOffset = GetFile(Assets, Asset->FileIndex)->FontBitmapIDOffset;
                Font->Glyphs = (hha_font_glyph*)(Asset->Header + 1);
                Font->HorizontalAdvance = (real32*)((uint8*)Font->Glyphs + GlyphsSize);
                Font->UnicodeMap = (uint16*)((uint8*)Font->HorizontalAdvance + HorizontalAdvanceSize);

                ZeroSize(UnicodeMapSize, Font->UnicodeMap);

                load_asset_work Work;
                Work.Task = Task;
                Work.Asset = Assets->Assets + ID.Value;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Offset = HHAAsset->DataOffset;
                Work.Size = SizeData;
                Work.Destination = Font->Glyphs;
                Work.FinalizeOperation = FinalizeAsset_Font;
                Work.FinalState = AssetState_Loaded;
                if (Task) {
                    load_asset_work* TaskWork = PushStruct(&Task->Arena, load_asset_work);
                    *TaskWork = Work;
                    Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, TaskWork);
                } else {
                    LoadAssetWorkDirectly(&Work);
                }
            } else {
                Asset->State = AssetState_Unloaded;
            }
        } else if (Immediate) {
            asset_state volatile* State = (asset_state volatile*)&Asset->State;
            while (*State == AssetState_Queued) {}
        }
    }
}

internal void LoadSound(game_assets* Assets, sound_id ID) {
    asset* Asset = Assets->Assets + ID.Value;
    if (ID.Value && AtomicCompareExchangeUint32((uint32*)&Asset->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded) {
        task_with_memory* Task = BeginTaskWithMemory(Assets->TranState);
        if (Task) {
            hha_asset* HHAAsset = &Asset->HHA;
            hha_sound* Info = &HHAAsset->Sound;

            asset_memory_size Size = {};
            Size.Section = Info->SampleCount * sizeof(int16);
            Size.Data = Info->ChannelCount * Size.Section;
            Size.Total = Size.Data + sizeof(asset_memory_header);

            Asset->Header = (asset_memory_header*)AcquireAssetMemory(Assets, Size.Total, ID.Value);

            loaded_sound* Sound = &Asset->Header->Sound;
            Sound->SampleCount = Info->SampleCount;
            Sound->ChannelCount = Info->ChannelCount;
            void* Memory = Asset->Header + 1;

            int16* SoundAt = (int16*)Memory;
            for (uint32 ChannelIndex = 0; ChannelIndex < Sound->ChannelCount; ++ChannelIndex) {
                Sound->Samples[ChannelIndex] = SoundAt;
                SoundAt += Sound->SampleCount;
            }

            load_asset_work* Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Asset = Assets->Assets + ID.Value;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Offset = HHAAsset->DataOffset;
            Work->Size = Size.Data;
            Work->Destination = Memory;
            Work->FinalizeOperation = FinalizeAsset_None;
            Work->FinalState = AssetState_Loaded;
#if 1
            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
#else
            LoadAssetWork(Assets->TranState->LowPriorityQueue, Work);
#endif
        } else {
            Asset->State = AssetState_Unloaded;
        }
    }
}

internal void PrefetchSound(game_assets* Assets, sound_id ID) {
    LoadSound(Assets, ID);
}

internal void PrefetchBitmap(game_assets* Assets, bitmap_id ID) {
    LoadBitmap(Assets, ID, false);
}

struct fill_ground_chunk_work {
    game_state* GameState;
    transient_state* TranState;
    ground_buffer* GroundBuffer;
    world_position ChunkP;
    task_with_memory* Task;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(FillGroundChunkWork) {
    TIMED_FUNCTION();

    fill_ground_chunk_work* Work = (fill_ground_chunk_work*)Data;

    loaded_bitmap* Buffer = &Work->GroundBuffer->Bitmap;
    Buffer->AlignPercentage = V2(0.5f, 0.5f);
    Buffer->WidthOverHeight = 1.0f;

    real32 Width = Work->GameState->World->ChunkDimInMeters.x;
    real32 Height = Work->GameState->World->ChunkDimInMeters.y;
    Assert(Width == Height);
    v2 HalfDim = 0.5f * V2(Width, Height);

    render_group* RenderGroup = AllocateRenderGroup(Work->TranState->Assets, &Work->Task->Arena, 0, true);
    BeginRender(RenderGroup);

    Orthographic(RenderGroup, Buffer->Width, Buffer->Height, (real32)(Buffer->Width - 2) / Width);
    Clear(RenderGroup, V4(1.0f, 0.5f, 0.0f, 1.0f));

    for (int32 ChunkOffsetY = -1; ChunkOffsetY <= 1; ChunkOffsetY++) {
        for (int32 ChunkOffsetX = -1; ChunkOffsetX <= 1; ChunkOffsetX++) {

            int32 ChunkX = Work->ChunkP.ChunkX + ChunkOffsetX;
            int32 ChunkY = Work->ChunkP.ChunkY + ChunkOffsetY;
            int32 ChunkZ = Work->ChunkP.ChunkZ;

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

                bitmap_id Stamp = GetRandomBitmapFrom(Work->TranState->Assets, RandomChoice(&Series, 2) ? Asset_Grass : Asset_Stone, &Series);;

                v2 Offset =
                    Hadamard(HalfDim, V2(RandomBilateral(&Series), RandomBilateral(&Series)));
                v2 P = Center + Offset;
                PushBitmap(RenderGroup, Stamp, 2.0f, V3(P, 0), Color);
            }
        }
    }

    for (int32 ChunkOffsetY = -1; ChunkOffsetY <= 1; ChunkOffsetY++) {
        for (int32 ChunkOffsetX = -1; ChunkOffsetX <= 1; ChunkOffsetX++) {

            int32 ChunkX = Work->ChunkP.ChunkX + ChunkOffsetX;
            int32 ChunkY = Work->ChunkP.ChunkY + ChunkOffsetY;
            int32 ChunkZ = Work->ChunkP.ChunkZ;

            random_series Series = RandomSeed(139 * ChunkX + 593 * ChunkY + 329 * ChunkZ);

            v2 Center = V2(ChunkOffsetX * Width, ChunkOffsetY * Height);

            for (uint32 GrassIndex = 0; GrassIndex < 50; ++GrassIndex) {
                bitmap_id Stamp = GetRandomBitmapFrom(Work->TranState->Assets, Asset_Tuft, &Series);

                v2 Offset =
                    Hadamard(HalfDim, V2(RandomBilateral(&Series), RandomBilateral(&Series)));
                v2 P = Center + Offset;
                PushBitmap(RenderGroup, Stamp, 0.1f, V3(P, 0));
            }
        }
    }

    Assert(AllResourcesPresent(RenderGroup));
    RenderGroupToOutput(RenderGroup, Buffer);
    EndRender(RenderGroup);
    EndTaskWithMemory(Work->Task);
}

internal void FillGroundChunk(
    transient_state* TranState, game_state* GameState,
    ground_buffer* GroundBuffer, world_position* ChunkP
) {
    task_with_memory* Task = BeginTaskWithMemory(TranState);
    if (Task) {
        fill_ground_chunk_work* Work = PushStruct(&Task->Arena, fill_ground_chunk_work);
        Work->ChunkP = *ChunkP;
        Work->GameState = GameState;
        Work->TranState = TranState;
        Work->GroundBuffer = GroundBuffer;
        Work->Task = Task;
        GroundBuffer->P = *ChunkP;
        Platform.AddEntry(TranState->LowPriorityQueue, FillGroundChunkWork, Work);
    }
}

struct hero_bitmap_ids {
    bitmap_id Head;
    bitmap_id Cape;
    bitmap_id Torso;
};

global_variable render_group* DEBUGRenderGroup;
global_variable real32 LeftEdge;
global_variable real32 AtY;
global_variable real32 FontScale;
global_variable font_id DEBUGFontID;
global_variable real32 GlobalWidth;
global_variable real32 GlobalHeight;

internal void
DEBUGReset(uint32 Width, uint32 Height) {
    GlobalWidth = (real32)Width;
    GlobalHeight = (real32)Height;
    FontScale = 0.25f;
    Orthographic(DEBUGRenderGroup, Width, Height, 1.0f);
    asset_vector MatchVector = {};
    MatchVector.E[Tag_FontType] = FontType_Debug;
    asset_vector WeightVector = {};
    WeightVector.E[Tag_FontType] = 1.0f;
    DEBUGFontID = GetBestMatchFontFrom(DEBUGRenderGroup->Assets, Asset_Font, &MatchVector, &WeightVector);
    hha_font* FontInfo = GetFontInfo(DEBUGRenderGroup->Assets, DEBUGFontID);
    AtY = (real32)Height * 0.5f - FontScale * GetAscenderHeightFor(FontInfo);
    LeftEdge = -(real32)Width * 0.5f;
}

internal void
DEBUGTextOutAt(v2 P, char* String) {
    if (DEBUGRenderGroup) {
        loaded_font* Font = PushFont(DEBUGRenderGroup, DEBUGFontID);
        if (Font) {
            hha_font* FontInfo = GetFontInfo(DEBUGRenderGroup->Assets, DEBUGFontID);
            uint32 PrevCodepoint = 0;
            real32 AtX = P.x;
            real32 AtY_ = P.y;
            real32 CharScale = FontScale;
            for (char* At = String; *At; ++At) {
                uint32 Codepoint = *At;
                real32 AdvanceX = CharScale * GetHorizontalAdvanceForPair(FontInfo, Font, PrevCodepoint, Codepoint);
                AtX += AdvanceX;
                if (Codepoint != ' ') {
                    bitmap_id BitmapID = GetBitmapForGlyph(FontInfo, Font, Codepoint);
                    hha_bitmap* BitmapInfo = GetBitmapInfo(DEBUGRenderGroup->Assets, BitmapID);
                    PushBitmap(DEBUGRenderGroup, BitmapID, CharScale * (real32)BitmapInfo->Dim[1], V3(AtX, AtY_, 0), V4(1, 1, 1, 1));
                }
                PrevCodepoint = Codepoint;
            }
        }
    }
}

internal void DEBUGTextLine(char* String) {
    if (DEBUGRenderGroup) {
        loaded_font* Font = PushFont(DEBUGRenderGroup, DEBUGFontID);
        if (Font) {
            hha_font* FontInfo = GetFontInfo(DEBUGRenderGroup->Assets, DEBUGFontID);
            real32 AtX = LeftEdge;
            DEBUGTextOutAt(V2(AtX, AtY), String);
            AtY -= GetLineAdvanceFor(FontInfo) * FontScale;
        }
    }
}

internal void DEBUGOwl() {
    if (DEBUGRenderGroup) {
        loaded_font* Font = PushFont(DEBUGRenderGroup, DEBUGFontID);
        if (Font) {
            hha_font* FontInfo = GetFontInfo(DEBUGRenderGroup->Assets, DEBUGFontID);
            uint32 PrevCodepoint = 0;
            real32 AtX = LeftEdge;
            real32 CharScale = FontScale;
            uint32 OwlCodepoints[4] = {
                0x5c0f,
                0x8033,
                0x6728,
                0x514e,
            };
            for (uint32 CodepointIndex = 0; CodepointIndex < ArrayCount(OwlCodepoints); ++CodepointIndex) {
                uint32 Codepoint = OwlCodepoints[CodepointIndex];
                real32 AdvanceX = CharScale * GetHorizontalAdvanceForPair(FontInfo, Font, PrevCodepoint, Codepoint);
                AtX += AdvanceX;
                bitmap_id BitmapID = GetBitmapForGlyph(FontInfo, Font, Codepoint);
                hha_bitmap* BitmapInfo = GetBitmapInfo(DEBUGRenderGroup->Assets, BitmapID);
                PushBitmap(DEBUGRenderGroup, BitmapID, CharScale * (real32)BitmapInfo->Dim[1], V3(AtX, AtY, 0), V4(1, 1, 1, 1));
                PrevCodepoint = Codepoint;
            }
            AtY -= GetLineAdvanceFor(FontInfo) * FontScale;
        }
    }
}

#include "stdio.h"

struct debug_statistic {
    uint32 Count;
    real64 Min;
    real64 Max;
    real64 Avg;
};

internal void BeginDebugStatistic(debug_statistic* Stat) {
    Stat->Min = Real64Maximum;
    Stat->Max = -Real64Maximum;
    Stat->Avg = 0;
    Stat->Count = 0;
}

internal void AccumDebugStatistic(debug_statistic* Stat, real64 Value) {
    Stat->Avg += Value;
    Stat->Count++;
    if (Value < Stat->Min) {
        Stat->Min = Value;
    }
    if (Value > Stat->Max) {
        Stat->Max = Value;
    }
}

internal void EndDebugStatistic(debug_statistic* Stat) {
    if (Stat->Count != 0) {
        Stat->Avg /= (real64)Stat->Count;
    } else {
        Stat->Max = Stat->Min = 0;
    }
}

internal void
RefreshCollation(debug_state* DebugState);

internal void DEBUGOverlay(game_memory* Memory, game_input* Input) {
    debug_state* DebugState = (debug_state*)Memory->DebugStorage;
    if (DebugState && DEBUGRenderGroup) {

        debug_record* HotRecord = 0;

        v2 MouseP = V2(Input->MouseX, Input->MouseY);
        if (WasPressed(Input->MouseButtons[PlatformMouseButton_Right])) {
            DebugState->Paused = !DebugState->Paused;
        }
        loaded_font* Font = PushFont(DEBUGRenderGroup, DEBUGFontID);
        if (Font) {
            hha_font* FontInfo = GetFontInfo(DEBUGRenderGroup->Assets, DEBUGFontID);
#if 0
            for (uint32 CounterIndex = 0; CounterIndex < DebugState->CounterCount; CounterIndex++) {


                debug_counter_state* Counter = DebugState->CounterStates + CounterIndex;

                debug_statistic HitCount;
                debug_statistic CycleCount;
                debug_statistic CyclesPerHit;

                BeginDebugStatistic(&HitCount);
                BeginDebugStatistic(&CycleCount);
                BeginDebugStatistic(&CyclesPerHit);

                for (uint32 SnapshotIndex = 0; SnapshotIndex < DEBUG_SNAPSHOT_COUNT; ++SnapshotIndex) {
                    AccumDebugStatistic(&HitCount, (real64)Counter->Snapshots[SnapshotIndex].HitCount);
                    AccumDebugStatistic(&CycleCount, (real64)Counter->Snapshots[SnapshotIndex].CycleCount);
                    real64 CPH = 0;
                    if (Counter->Snapshots[SnapshotIndex].HitCount) {
                        CPH = (real64)Counter->Snapshots[SnapshotIndex].CycleCount / (real64)Counter->Snapshots[SnapshotIndex].HitCount;
                    }
                    AccumDebugStatistic(&CyclesPerHit, CPH);
                }

                EndDebugStatistic(&HitCount);
                EndDebugStatistic(&CycleCount);
                EndDebugStatistic(&CyclesPerHit);

                if (Counter->BlockName) {

                    char TextBuffer[256];
                    _snprintf_s(
                        TextBuffer, sizeof(TextBuffer),
                        "%20s(%4d): %10ucy %10uh %10ucy/h\n",
                        Counter->BlockName, Counter->Linenumber, (uint32)CycleCount.Avg, (uint32)HitCount.Avg, (uint32)(CyclesPerHit.Avg)
                    );

                    if (CycleCount.Max > 0) {
                        real32 BarWidth = 4.0f;
                        real32 ChartLeft = 0.0f;
                        real32 ChartMinY = AtY;
                        real32 ChartHeight = FontInfo->AscenderHeight * FontScale;
                        real32 Scale = 1.0f / (real32)CycleCount.Max;
                        for (uint32 SnapshotIndex = 0; SnapshotIndex < DEBUG_SNAPSHOT_COUNT; ++SnapshotIndex) {
                            real32 ThisProportion = Scale * (real32)Counter->Snapshots[SnapshotIndex].CycleCount;
                            real32 ThisHeight = ChartHeight * ThisProportion;
                            PushRect(DEBUGRenderGroup, V3(ChartLeft + (real32)SnapshotIndex * (BarWidth + 1.0f) + 0.5f * BarWidth, ChartMinY + 0.5f * ThisHeight, 0.0f), V2(BarWidth, ThisHeight), V4(ThisProportion, 1.0f, 0.0f, 1.0f));
                        }
                    }
                    DEBUGTextLine(TextBuffer);
                }
            }
#endif
            if (DebugState->FrameCount) {
                char TextBuffer[256];
                _snprintf_s(
                    TextBuffer, sizeof(TextBuffer),
                    "Last frame time: %.02fms\n",
                    DebugState->Frames[DebugState->FrameCount - 1].WallSecondsElapsed * 1000.0f
                );
                DEBUGTextLine(TextBuffer);
            }
#if 1
            real32 LaneHeight = 20.0f;
            real32 LaneCount = (real32)DebugState->FrameBarLaneCount;
            real32 BarHeight = LaneHeight * LaneCount;
            real32 BarSpacing = BarHeight + 4.0f;
            real32 ChartLeft = LeftEdge + 10.0f;
            real32 ChartHeight = BarSpacing * (real32)DebugState->FrameCount;
            real32 ChartWidth = 1000.0f;
            real32 ChartTop = 0.5f * GlobalHeight - 10.0f;
            real32 Scale = ChartWidth * DebugState->FrameBarScale;

            v3 Colors[] = {
              {1, 0, 0},
              {0, 1, 0},
              {0, 0, 1},
              {1, 1, 0},
              {0, 1, 1},
              {1, 0, 1},
              {1, 0.5f, 0},
              {1, 0, 0.5f},
              {0.5f, 1, 0},
              {0, 1, 0.5f},
              {0.5f, 0, 1},
              {0, 0.5f, 1.0f},
            };


            uint32 MaxFrame = DebugState->FrameCount;
            if (MaxFrame > 10) {
                MaxFrame = 10;
            }
            for (uint32 FrameIndex = 0; FrameIndex < MaxFrame; ++FrameIndex) {

                debug_frame* Frame = DebugState->Frames + DebugState->FrameCount - (FrameIndex + 1);
                real32 StackX = ChartLeft;
                real32 StackY = ChartTop - BarSpacing * (real32)FrameIndex;

                for (uint32 RegionIndex = 0; RegionIndex < Frame->RegionCount; ++RegionIndex) {

                    debug_frame_region* Region = Frame->Regions + RegionIndex;

                    //v3 Color = Colors[RegionIndex % ArrayCount(Colors)];
                    v3 Color = Colors[Region->ColorIndex % ArrayCount(Colors)];
                    real32 ThisMinX = StackX + Scale * Region->MinT;
                    real32 ThisMaxX = StackX + Scale * Region->MaxT;

                    rectangle2 RegionRect = RectMinMax(
                        V2(ThisMinX, StackY - LaneHeight * (Region->LaneIndex + 1)),
                        V2(ThisMaxX, StackY - LaneHeight * Region->LaneIndex)
                    );

                    PushRect(
                        DEBUGRenderGroup,
                        RegionRect, 0.0f,
                        V4(Color, 1.0f)
                    );

                    if (IsInRectangle(RegionRect, MouseP)) {
                        debug_record* Record = Region->Record;
                        char TextBuffer[256];
                        _snprintf_s(
                            TextBuffer, sizeof(TextBuffer),
                            "%s: %10Iucy [%s(%d)]",
                            Record->BlockName,
                            Region->CycleCount,
                            Record->Filename, Record->Linenumber
                        );
                        DEBUGTextOutAt(MouseP + V2(0.0f, 10.f), TextBuffer);

                        HotRecord = Record;
                    }
                }
            }
#if 0
            PushRect(
                DEBUGRenderGroup,
                V3(ChartLeft + 0.5f * ChartWidth,
                    ChartMinY + ChartHeight,
                    0.0f),
                V2(ChartWidth, 4.0f),
                V4(1.0f, 1.0f, 1.0f, 1.0f)
            );
#endif
#endif
        }

        if (WasPressed(Input->MouseButtons[PlatformMouseButton_Left])) {
            if (HotRecord) {
                DebugState->ScopeToRecord = HotRecord;
            } else if (DebugState->ScopeToRecord) {
                DebugState->ScopeToRecord = 0;
            }
            RefreshCollation(DebugState);
        }
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
    Platform = Memory->PlatformAPI;
#if HANDMADE_INTERNAL
    //  DebugGlobalMemory = Memory;
#endif
    TIMED_FUNCTION();
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

        TranState->Assets = AllocateGameAssets(&TranState->TranArena, Megabytes(64), TranState);

        DEBUGRenderGroup = AllocateRenderGroup(TranState->Assets, &TranState->TranArena, Megabytes(16), false);

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

    if (DEBUGRenderGroup) {
        BeginRender(DEBUGRenderGroup);
        DEBUGReset(Buffer->Width, Buffer->Height);
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
    DrawBuffer->Height = Buffer->Height;
    DrawBuffer->Pitch = Buffer->Pitch;
    DrawBuffer->Width = Buffer->Width;
    DrawBuffer->Memory = Buffer->Memory;

    render_group* RenderGroup = AllocateRenderGroup(TranState->Assets, &TranState->TranArena, Megabytes(4), false);
    BeginRender(RenderGroup);

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

        asset_vector MatchVector = {};
        asset_vector WeightVector = {};

        char Nothings[] = "NOTHINGS";

        MatchVector.E[Tag_UnicodeCodepoint] = (real32)Nothings[RandomChoice(&GameState->EffectsEntropy, ArrayCount(Nothings) - 1)];
        WeightVector.E[Tag_UnicodeCodepoint] = 1.0f;

        Particle->BitmapID = GetBestMatchBitmapFrom(TranState->Assets, Asset_Font, &MatchVector, &WeightVector);
        // Particle->BitmapID = GetRandomBitmapFrom(TranState->Assets, Asset_Font, &GameState->EffectsEntropy);
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

#if 0
    for (uint32 Y = 0; Y < PARTICLE_CELL_DIM; ++Y) {
        for (uint32 X = 0; X < PARTICLE_CELL_DIM; ++X) {
            particle_cell* Cell = &GameState->ParticleCells[Y][X];
            real32 Alpha = Clamp01(Cell->Density * 0.1f);
            PushRect(RenderGroup, V3i(X, Y, 0) * GridScale + GridOrigin, GridScale * V2(1.0f, 1.0f), V4(Alpha, Alpha, Alpha, Alpha));
        }
    }
#endif

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

        PushBitmap(RenderGroup, Particle->BitmapID, 0.3f, Particle->P, Color);
    }
#endif

#if 0
    local_persist bool32 temp = true;
    local_persist bitmap_id TempBitmap;
    if (temp) {
        TempBitmap = GetRandomBitmapFrom(TranState->Assets, Asset_Font, &GameState->EffectsEntropy);
        temp = false;
    }
    PushBitmap(DEBUGRenderGroup, TempBitmap, 60.0f, V3(7.0f, 0.0f, 0.0f), V4(1, 1, 1, 1));
#endif

    TiledRenderGroupToOutput(TranState->HighPriorityQueue, RenderGroup, DrawBuffer);
    EndRender(RenderGroup);

    EndSim(SimRegion, GameState);

    EndTemporaryMemory(SimMemory);
    EndTemporaryMemory(RenderMemory);

    CheckArena(&GameState->WorldArena);
    CheckArena(&TranState->TranArena);

    //END_TIMED_BLOCK(GameUpdateAndRender);

    DEBUGOverlay(Memory, Input);

    if (DEBUGRenderGroup) {
        TiledRenderGroupToOutput(TranState->HighPriorityQueue, DEBUGRenderGroup, DrawBuffer);
        EndRender(DEBUGRenderGroup);
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    transient_state* TranState = (transient_state*)Memory->TransientStorage;
    OutputPlayingSounds(
        &GameState->AudioState, SoundBuffer, TranState->Assets, &TranState->TranArena
    );
}

#define DebugRecords_Main_Count __COUNTER__

global_variable debug_table GlobalDebugTable_;
debug_table* GlobalDebugTable = &GlobalDebugTable_;

internal inline uint32
GetLaneFromThreadIndex(debug_state* DebugState, uint32 ThreadIndex) {
    uint32 Result = 0;
    return Result;
}

internal debug_thread*
GetDebugThread(debug_state* DebugState, uint32 ThreadID) {
    debug_thread* Result = 0;
    for (debug_thread* Thread = DebugState->FirstThread; Thread; Thread = Thread->Next) {
        if (Thread->ID == ThreadID) {
            Result = Thread;
            break;
        }
    }
    if (!Result) {
        Result = PushStruct(&DebugState->CollateArena, debug_thread);
        Result->FirstOpenBlock = 0;
        Result->LaneIndex = DebugState->FrameBarLaneCount++;
        Result->ID = ThreadID;
        Result->Next = DebugState->FirstThread;
        DebugState->FirstThread = Result;
    }
    return Result;
}

internal debug_frame_region*
AddRegion(debug_state* DebugState, debug_frame* CurrentFrame) {
    Assert(CurrentFrame->RegionCount < MAX_REGIONS_PER_FRAME);
    debug_frame_region* Result = CurrentFrame->Regions + CurrentFrame->RegionCount++;
    return Result;
}

internal inline debug_record*
GetRecordFrom(open_debug_block* Block) {
    debug_record* Result = Block ? Block->Source : 0;
    return Result;
}

internal void
CollateDebugRecords(debug_state* DebugState, uint32 InvalidEventArrayIndex) {

    for (;
        ;
        ++DebugState->CollationArrayIndex) {

        if (DebugState->CollationArrayIndex == MAX_DEBUG_EVENT_ARRAY_COUNT) {
            DebugState->CollationArrayIndex = 0;
        }
        if (DebugState->CollationArrayIndex == InvalidEventArrayIndex) {
            break;
        }

        uint32 EventCount = GlobalDebugTable->EventCount[DebugState->CollationArrayIndex];
        debug_event* Events = GlobalDebugTable->Events[DebugState->CollationArrayIndex];

        for (uint32 EventIndex = 0; EventIndex < EventCount; ++EventIndex) {

            debug_event* Event = Events + EventIndex;

            debug_record* Source = GlobalDebugTable->Records[Event->TranslationUnit] + Event->DebugRecordIndex;

            if (Event->Type == DebugEvent_FrameMarker) {

                if (DebugState->CollationFrame) {
                    DebugState->CollationFrame->EndClock = Event->Clock;
                    DebugState->CollationFrame->WallSecondsElapsed = Event->SecondsElapsed;
                    ++DebugState->FrameCount;
                    real32 ClockRange = (real32)(DebugState->CollationFrame->EndClock - DebugState->CollationFrame->BeginClock);
#if 0
                    if (ClockRange > 0) {
                        real32 FrameBarScale = 1.0f / ClockRange;
                        if (DebugState->FrameBarScale > FrameBarScale) {
                            DebugState->FrameBarScale = FrameBarScale;
                        }
                    }
#endif
                }

                DebugState->CollationFrame = DebugState->Frames + DebugState->FrameCount;
                DebugState->CollationFrame->BeginClock = Event->Clock;
                DebugState->CollationFrame->EndClock = 0;
                DebugState->CollationFrame->RegionCount = 0;
                DebugState->CollationFrame->Regions = PushArray(&DebugState->CollateArena, MAX_REGIONS_PER_FRAME, debug_frame_region);
                DebugState->CollationFrame->WallSecondsElapsed = 0.0f;

            } else if (DebugState->CollationFrame) {

                uint32 FrameIndex = DebugState->FrameCount - 1;
                debug_thread* Thread = GetDebugThread(DebugState, Event->TC.ThreadID);
                uint64 RelativeClock = Event->Clock - DebugState->CollationFrame->BeginClock;

                if (Event->Type == DebugEvent_BeginBlock) {

                    open_debug_block* DebugBlock = DebugState->FirstFreeBlock;
                    if (DebugBlock) {
                        DebugState->FirstFreeBlock = DebugBlock->NextFree;
                    } else {
                        DebugBlock = PushStruct(&DebugState->CollateArena, open_debug_block);
                    }
                    DebugBlock->StartingFrameIndex = FrameIndex;
                    DebugBlock->OpeningEvent = Event;
                    DebugBlock->Parent = Thread->FirstOpenBlock;
                    DebugBlock->Source = Source;
                    Thread->FirstOpenBlock = DebugBlock;
                    DebugBlock->NextFree = 0;

                } else if (Event->Type == DebugEvent_EndBlock) {

                    if (Thread->FirstOpenBlock) {

                        open_debug_block* MatchingBlock = Thread->FirstOpenBlock;
                        debug_event* OpeningEvent = MatchingBlock->OpeningEvent;

                        if (OpeningEvent->TC.ThreadID == Event->TC.ThreadID &&
                            OpeningEvent->DebugRecordIndex == Event->DebugRecordIndex &&
                            OpeningEvent->TranslationUnit == Event->TranslationUnit) {

                            if (MatchingBlock->StartingFrameIndex == FrameIndex) {

                                if (GetRecordFrom(MatchingBlock->Parent) == DebugState->ScopeToRecord) {

                                    real32 MinT = (real32)(OpeningEvent->Clock - DebugState->CollationFrame->BeginClock);
                                    real32 MaxT = (real32)(Event->Clock - DebugState->CollationFrame->BeginClock);
                                    real32 ThresholdT = 0.01f;
                                    if (MaxT - MinT > ThresholdT) {
                                        debug_frame_region* Region = AddRegion(DebugState, DebugState->CollationFrame);
                                        Region->LaneIndex = (uint16)Thread->LaneIndex;
                                        Region->MinT = MinT;
                                        Region->MaxT = MaxT;
                                        Region->ColorIndex = OpeningEvent->DebugRecordIndex;
                                        Region->CycleCount = Event->Clock - OpeningEvent->Clock;
                                        Region->Record = Source;
                                    }

                                }
                            } else {

                            }

                            Thread->FirstOpenBlock->NextFree = DebugState->FirstFreeBlock;
                            DebugState->FirstFreeBlock = Thread->FirstOpenBlock;
                            Thread->FirstOpenBlock = MatchingBlock->Parent;

                        } else {

                        }
                    }
                } else {
                    Assert(!"Invalid event type");
                }
            }
        }
    }
}

internal void
RestartCollation(debug_state* DebugState, uint32 InvalidEventArrayIndex) {
    EndTemporaryMemory(DebugState->CollateTemp);
    DebugState->CollateTemp = BeginTemporaryMemory(&DebugState->CollateArena);

    DebugState->FirstThread = 0;
    DebugState->FirstFreeBlock = 0;

    DebugState->Frames = PushArray(&DebugState->CollateArena, MAX_DEBUG_EVENT_ARRAY_COUNT * 4, debug_frame);
    DebugState->FrameBarLaneCount = 0;
    DebugState->FrameCount = 0;
    DebugState->FrameBarScale = 1.0f / (60000000.0f);

    DebugState->CollationArrayIndex = InvalidEventArrayIndex + 1;
    DebugState->CollationFrame = 0;
}

internal void
RefreshCollation(debug_state* DebugState) {
    uint32 ArrayIndex = GlobalDebugTable->EventArrayIndex_EventIndex >> 32;
    RestartCollation(DebugState, ArrayIndex);
    CollateDebugRecords(DebugState, ArrayIndex);
}

extern "C" DEBUG_GAME_FRAME_END(DEBUGGameFrameEnd) {

    GlobalDebugTable->RecordCount[TRANSLATION_UNIT_INDEX] = DebugRecords_Main_Count;

    uint64 EventArrayIndex = GlobalDebugTable->EventArrayIndex_EventIndex >> 32;
    uint64 NextArrayIndex = EventArrayIndex + 1;
    if (NextArrayIndex >= ArrayCount(GlobalDebugTable->Events)) {
        NextArrayIndex = 0;
    }
    uint64 ArrayIndex_EventIndex = AtomicExchangeU64(&GlobalDebugTable->EventArrayIndex_EventIndex, NextArrayIndex << 32);
    Assert((ArrayIndex_EventIndex >> 32) == EventArrayIndex);
    uint32 EventCount = ArrayIndex_EventIndex & 0xFFFFFFFF;
    GlobalDebugTable->EventCount[EventArrayIndex] = EventCount;

    debug_state* DebugState = (debug_state*)Memory->DebugStorage;
    if (DebugState) {
        if (!DebugState->Initialized) {
            InitializeArena(
                &DebugState->CollateArena, Memory->DebugStorageSize - sizeof(debug_state),
                DebugState + 1
            );
            DebugState->CollateTemp = BeginTemporaryMemory(&DebugState->CollateArena);

            DebugState->Paused = false;
            DebugState->ScopeToRecord = 0;

            DebugState->Initialized = true;
            RestartCollation(DebugState, (uint32)NextArrayIndex);
        }

        if (!DebugState->Paused) {
            if (DebugState->FrameCount >= MAX_DEBUG_EVENT_ARRAY_COUNT * 4) {
                RestartCollation(DebugState, (uint32)NextArrayIndex);
            }
            CollateDebugRecords(DebugState, (uint32)NextArrayIndex);
        }
    }

    return GlobalDebugTable;
}
