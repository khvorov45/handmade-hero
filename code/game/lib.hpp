#if !defined(HANDMADE_H)
#define HANDMADE_H

/**
 * HANDMADE_INTERNAL:
 *  0 - Public release
 *  1 - Developer
 *
 * HANDMADE_SLOW:
 *  0 - No slow code allowed
 *  1 - Slow code allowed
*/

#include "../types.h"
#include "../intrinsics.h"

#define Pi32 3.14159265358979323846264338327950f

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Kilobytes(n) (((uint64)(n)) * 1024)
#define Megabytes(n) (Kilobytes(n) * 1024)
#define Gigabytes(n) (Megabytes(n) * 1024)
#define Terabytes(n) (Gigabytes(n) * 1024)

#if HANDMADE_SLOW
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

struct thread_context {
    int32 Placeholder;
};

#if HANDMADE_INTERNAL
struct debug_read_file_result {
    void* Contents;
    uint32 Size;
};

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context* Thread, char* Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context* Thread, void* BitmapMemory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(thread_context* Thread, char* Filename, uint32 MemorySize, void* Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#else
#endif

struct tile_chunk_position {
    uint32 TileChunkX;
    uint32 TileChunkY;

    uint32 RelTileX;
    uint32 RelTileY;
};

struct world_position {
    //* High bits - tile chunk index
    //* Low bits - tile location in the chunk
    uint32 AbsTileX;
    uint32 AbsTileY;

    real32 XTileRel;
    real32 YTileRel;
};

struct game_state {
#if 0
    real32 PlayerX;
    real32 PlayerY;

    int32 PlayerTileMapX;
    int32 PlayerTileMapY;
#endif
    world_position PlayerP;
};

struct game_memory {
    bool32 IsInitialized;
    uint64 PermanentStorageSize;
    void* PermanentStorage; //* Required to be cleared to 0
    uint64 TransientStorageSize;
    void* TransientStorage; //* Required to be cleared to 0

    debug_platform_read_entire_file* DEBUGPlatformReadEntireFile;
    debug_platform_free_file_memory* DEBUGPlatformFreeFileMemory;
    debug_platform_write_entire_file* DEBUGPlatformWriteEntireFile;
};

struct game_offscreen_buffer {
    void* Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct game_sound_buffer {
    int16* Samples;
    int32 SamplesPerSecond;
    int32 SampleCount;
};

struct game_button_state {
    int32 HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input {
    bool32 IsConnected;
    bool32 IsAnalog;
    real32 StickAverageX;
    real32 StickAverageY;

    union {
        game_button_state Buttons[12];
        struct {
            game_button_state MoveUp;
            game_button_state MoveDown;
            game_button_state MoveLeft;
            game_button_state MoveRight;

            game_button_state ActionUp;
            game_button_state ActionDown;
            game_button_state ActionLeft;
            game_button_state ActionRight;

            game_button_state LeftShoulder;
            game_button_state RightShoulder;

            game_button_state Start;
            game_button_state Back;

            // Add buttons above this
            game_button_state Terminator;
        };
    };
};

struct game_input {
    game_button_state MouseButtons[5];
    int32 MouseX, MouseY, MouseZ;
    real32 dtForFrame;
    game_controller_input Controllers[5];
};

internal inline game_controller_input* GetController(game_input* Input, int ControllerIndex) {
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    return &Input->Controllers[ControllerIndex];
}

inline uint32 SafeTruncateUint64(uint64 Value) {
    Assert(Value <= 0xFFFFFFFF);
    return (uint32)(Value);
}

#define GAME_UPDATE_AND_RENDER(name) void name(thread_context* Thread, game_memory* Memory, game_input* Input, game_offscreen_buffer* Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context* Thread, game_memory* Memory, game_sound_buffer* SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#endif
