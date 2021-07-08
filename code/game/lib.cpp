#include "lib.hpp"
#include "world.cpp"
#include "state.cpp"
#include "sim_region.cpp"
#include "entity.cpp"
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

internal void DrawBitmap(
    game_offscreen_buffer* Buffer, loaded_bitmap* BMP,
    real32 RealStartX, real32 RealStartY,
    real32 CAlpha = 1.0f
) {

    int32 StartX = RoundReal32ToInt32(RealStartX);
    int32 StartY = RoundReal32ToInt32(RealStartY);

    int32 EndX = StartX + BMP->Width;
    int32 EndY = StartY + BMP->Height;

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

    uint32* SourceRow =
        BMP->Pixels + BMP->Width * (BMP->Height - 1) - SourceOffsetY * BMP->Width + SourceOffsetX;
    uint8* DestRow = (uint8*)Buffer->Memory + StartY * Buffer->Pitch + StartX * Buffer->BytesPerPixel;
    for (int32 Y = StartY; Y < EndY; ++Y) {
        uint32* Dest = (uint32*)DestRow;
        uint32* Source = SourceRow;
        for (int32 X = StartX; X < EndX; ++X) {

            real32 A = (real32)((*Source >> 24)) / 255.0f * CAlpha;

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
        SourceRow -= BMP->Width;
    }
}

internal inline void
PushPiece(
    entity_visible_piece_group* Group, loaded_bitmap* Bitmap,
    v2 Offset, real32 OffsetZ, v2 Align, v2 Dim,
    v4 Color,
    real32 EntityZC = 1.0f
) {
    Assert(Group->PieceCount < ArrayCount(Group->Pieces));
    real32 MetersToPixels = Group->GameState->MetersToPixels;
    entity_visible_piece* Piece = Group->Pieces + Group->PieceCount++;

    v2 OffsetFixY = { Offset.X, -Offset.Y };

    Piece->R = Color.R;
    Piece->G = Color.G;
    Piece->B = Color.B;
    Piece->A = Color.A;
    Piece->Bitmap = Bitmap;
    Piece->Offset = MetersToPixels * OffsetFixY - Align;
    Piece->OffsetZ = OffsetZ;
    Piece->EntityZC = EntityZC;
    Piece->Dim = Dim;
}

internal inline void
PushBitmap(
    entity_visible_piece_group* Group, loaded_bitmap* Bitmap,
    v2 Offset, real32 OffsetZ, v2 Align, real32 Alpha = 1.0f,
    real32 EntityZC = 1.0f
) {
    PushPiece(
        Group, Bitmap, Offset, OffsetZ, Align, { 0, 0 }, { 0, 0, 0, Alpha }, EntityZC
    );
}

internal void
PushRect(
    entity_visible_piece_group* Group,
    v2 Offset, real32 OffsetZ, v2 Dim,
    v4 Color,
    real32 EntityZC = 1.0f
) {
    PushPiece(
        Group, 0, Offset, OffsetZ, { 0, 0 }, Dim, Color, EntityZC
    );
}

internal void DrawHitpoints_(sim_entity* Entity, entity_visible_piece_group* PieceGroup) {
    if (Entity->HitPointMax >= 1) {
        v2 HealthDim = { 0.2f, 0.2f };
        real32 SpacingX = 2.0f * HealthDim.X;
        v2 HitP = { (Entity->HitPointMax - 1) * (-0.5f) * SpacingX, -0.2f };
        v2 dHitP = { SpacingX, 0.0f };
        for (uint32 HealthIndex = 0; HealthIndex < Entity->HitPointMax; ++HealthIndex) {
            v4 Color = { 1.0f, 0.0f, 0.0f, 1.0f };
            hit_point* HitPoint = Entity->HitPoint + HealthIndex;
            if (HitPoint->FilledAmount == 0) {
                Color.R = 0.2f;
                Color.G = 0.2f;
                Color.B = 0.2f;
            }
            PushRect(PieceGroup, HitP, 0, HealthDim, Color, 0.0f);
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
        InitializeArena(
            &GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
            (uint8*)Memory->PermanentStorage + sizeof(game_state)
        );

        AddLowEntity_(GameState, EntityType_Null, NullPosition());

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world* TileMap = GameState->World;
        InitializeWorld(TileMap, 1.4f, 3.0f);

        uint32 TileSideInPixels = 60;
        GameState->MetersToPixels = (real32)TileSideInPixels / (real32)TileMap->TileSideInMeters;

        GameState->NullCollision = MakeNullCollision(GameState);
        GameState->SwordCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 0.1f);
        GameState->StairCollision = MakeSimpleGroundedCollision(
            GameState,
            GameState->World->TileSideInMeters,
            2.0f * GameState->World->TileSideInMeters,
            1.1f * GameState->World->TileDepthInMeters
        );
        GameState->PlayerCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 1.2f);
        GameState->MonsterCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 0.5f);
        GameState->FamiliarCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 0.5f);
        GameState->WallCollision = MakeSimpleGroundedCollision(
            GameState, GameState->World->TileSideInMeters,
            GameState->World->TileSideInMeters,
            GameState->World->TileDepthInMeters
        );

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

        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;

        uint32 ScreenBaseX = 0;
        uint32 ScreenBaseY = 0;
        uint32 ScreenBaseZ = 0;

        uint32 ScreenX = ScreenBaseX;
        uint32 ScreenY = ScreenBaseY;
        uint32 AbsTileZ = ScreenBaseZ;

        uint32 RandomNumberIndex = 0;

        bool32 DoorLeft = false;
        bool32 DoorRight = false;
        bool32 DoorTop = false;
        bool32 DoorBottom = false;
        bool32 DoorUp = false;
        bool32 DoorDown = false;

        for (uint32 ScreenIndex = 0; ScreenIndex < 2000; ++ScreenIndex) {

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
                if (AbsTileZ == ScreenBaseZ) {
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

            if (RandomChoice == 2) {
                if (AbsTileZ == ScreenBaseZ) {
                    AbsTileZ = ScreenBaseZ + 1;
                } else {
                    AbsTileZ = ScreenBaseZ;
                }
            } else if (RandomChoice == 1) {
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
                    ConHero->ddP.Y = 1;
                }
                if (Controller->MoveDown.EndedDown) {
                    ConHero->ddP.Y = -1;
                }
                if (Controller->MoveLeft.EndedDown) {
                    ConHero->ddP.X = -1;
                }
                if (Controller->MoveRight.EndedDown) {
                    ConHero->ddP.X = 1;
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

    //* SimRegion
    uint32 TileSpanX = 17 * 3;
    uint32 TileSpanY = 9 * 3;
    uint32 TileSpanZ = 1;
    rectangle3 CameraBounds = RectCenterDim(
        V3(0, 0, 0),
        World->TileSideInMeters * V3((real32)TileSpanX, (real32)TileSpanY, (real32)TileSpanZ)
    );

    memory_arena SimArena;
    InitializeArena(&SimArena, Memory->TransientStorageSize, Memory->TransientStorage);

    sim_region* SimRegion =
        BeginSim(&SimArena, GameState, World, GameState->CameraP, CameraBounds, Input->dtForFrame);

    //* Clear screen
    DrawRectangle(
        Buffer, { 0, 0 }, { (real32)Buffer->Width, (real32)Buffer->Height },
        1.0f, 0.0f, 1.0f
    );

    //* Draw backdrop
#if 0
    DrawBitmap(Buffer, GameState->Backdrop, 0.0f, 0.0f);
#else
    DrawRectangle(
        Buffer, { 0, 0 }, { (real32)Buffer->Width, (real32)Buffer->Height },
        0.5f, 0.5f, 0.5f
    );
#endif

    //* Draw entities
    real32 ScreenCenterX = (real32)Buffer->Width * 0.5f;
    real32 ScreenCenterY = (real32)Buffer->Height * 0.5f;
    entity_visible_piece_group PieceGroup;
    PieceGroup.GameState = GameState;
    for (uint32 EntityIndex = 0;
        EntityIndex < SimRegion->EntityCount;
        EntityIndex++) {

        sim_entity* Entity = SimRegion->Entities + EntityIndex;

        if (!Entity->Updatable) {
            continue;
        }

        real32 dt = Input->dtForFrame;

        real32 ShadowAlpha = Maximum(0.0f, 1.0f - 0.5f * Entity->P.Z);

        move_spec MoveSpec = DefaultMoveSpec();
        v3 ddP = {};

        hero_bitmaps* HeroBitmaps = &GameState->HeroBitmaps[Entity->FacingDirection];
        PieceGroup.PieceCount = 0;
        switch (Entity->Type) {
        case EntityType_Hero:
        {
            for (uint32 ControlIndex = 0;
                ControlIndex < ArrayCount(GameState->ControlledHeroes);
                ++ControlIndex) {

                controlled_hero* ConHero = GameState->ControlledHeroes + ControlIndex;

                if (ConHero->EntityIndex == Entity->StorageIndex) {
                    if (ConHero->dZ != 0.0f) {
                        Entity->dP.Z = ConHero->dZ;
                    }

                    MoveSpec.Drag = 8.0f;
                    MoveSpec.Speed = 150.0f;
                    MoveSpec.UnitMaxAccelVector = true;
                    ddP = V3(ConHero->ddP, 0.0f);

                    if (ConHero->dSword.X != 0.0f || ConHero->dSword.Y != 0.0f) {

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


            PushBitmap(&PieceGroup, &GameState->HeroShadow, { 0, 0 }, 0, HeroBitmaps->Align, ShadowAlpha, 0.0f);
            PushBitmap(&PieceGroup, &HeroBitmaps->Head, { 0, 0 }, 0, HeroBitmaps->Align);
            PushBitmap(&PieceGroup, &HeroBitmaps->Torso, { 0, 0 }, 0, HeroBitmaps->Align);
            PushBitmap(&PieceGroup, &HeroBitmaps->Cape, { 0, 0 }, 0, HeroBitmaps->Align);

            DrawHitpoints_(Entity, &PieceGroup);
        }
        break;
        case EntityType_Wall:
        {
            PushBitmap(&PieceGroup, &GameState->Tree, { 0, 0 }, 0, { 40, 80 });
        }
        break;
        case EntityType_Stairwell:
        {
            PushRect(&PieceGroup, V2(0, 0), 0, Entity->WalkableDim, V4(1, 0.5f, 0, 1), 0.0f);
            PushRect(&PieceGroup, V2(0, 0), Entity->WalkableHeight, Entity->WalkableDim, V4(1, 1, 0, 1), 0.0f);
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
            PushBitmap(&PieceGroup, &GameState->HeroShadow, { 0, 0 }, 0, HeroBitmaps->Align, ShadowAlpha, 0.0f);
            PushBitmap(&PieceGroup, &GameState->Sword, { 0, 0 }, 0, { 29, 10 });
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

            PushBitmap(&PieceGroup, &GameState->HeroShadow, { 0, 0 }, 0, HeroBitmaps->Align, ShadowAlpha * 0.5f + BobSin * 0.2f);
            PushBitmap(&PieceGroup, &HeroBitmaps->Head, { 0, 0 }, 0.2f * BobSin, HeroBitmaps->Align);
        }
        break;
        case EntityType_Monster:
        {
            PushBitmap(&PieceGroup, &GameState->HeroShadow, { 0, 0 }, 0, HeroBitmaps->Align, ShadowAlpha);
            PushBitmap(&PieceGroup, &HeroBitmaps->Torso, { 0, 0 }, 0, HeroBitmaps->Align);

            DrawHitpoints_(Entity, &PieceGroup);
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

        for (uint32 PieceIndex = 0; PieceIndex < PieceGroup.PieceCount; ++PieceIndex) {

            entity_visible_piece* Piece = PieceGroup.Pieces + PieceIndex;

            v3 EntityBaseP = GetEntityGroundPoint(Entity);
            real32 ZFudge = 1.0f + 0.1f * (EntityBaseP.Z + Piece->OffsetZ);

            real32 EntityGroudX = ScreenCenterX + EntityBaseP.X * GameState->MetersToPixels * ZFudge;
            real32 EntityGroudY = ScreenCenterY - EntityBaseP.Y * GameState->MetersToPixels * ZFudge;
            real32 EntityZ = -EntityBaseP.Z * GameState->MetersToPixels;

            v2 Center = {
                EntityGroudX + Piece->Offset.X,
                EntityGroudY + Piece->Offset.Y + EntityZ * Piece->EntityZC
            };

            if (Piece->Bitmap) {
                DrawBitmap(
                    Buffer,
                    Piece->Bitmap,
                    Center.X, Center.Y,
                    Piece->A
                );
            } else {
                v2 HalfDim = GameState->MetersToPixels * Piece->Dim * 0.5f;
                DrawRectangle(Buffer, Center - HalfDim, Center + HalfDim, Piece->R, Piece->G, Piece->B);
            }
        }
    }

    EndSim(SimRegion, GameState);
}
