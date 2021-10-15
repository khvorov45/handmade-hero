#if !defined(HANDMADE_H)
#define HANDMADE_H

//* Compilers
#if !defined(COMPILER_MSVC) && !defined(COMPILER_LLVM)
#if _MSC_VER
#define COMPILER_MSVC 1
#endif
#endif

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
#include "../util.h"

#if HANDMADE_INTERNAL
struct debug_read_file_result {
    void* Contents;
    uint32 Size;
};

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(char* Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void* BitmapMemory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(char* Filename, uint32 MemorySize, void* Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif

struct platform_work_queue;
#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue* Queue, void* Data)
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);
typedef void platform_add_entry(platform_work_queue* Queue, platform_work_queue_callback* Callback, void* Data);
typedef void platform_complete_all_work(platform_work_queue* Queue);

struct platform_file_handle {
    bool32 NoErrors;
    void* Platform;
};

struct platform_file_group {
    uint32 FileCount;
    void* Platform;
};

enum platform_file_type {
    PlatformFileType_AssetFile,
    PlatformFileType_SavedGameFile,
    PlatformFileType_Count,
};

#define PlatformNoFileErrors(Handle) ((Handle)->NoErrors)

#define PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(name) platform_file_group name(platform_file_type Type)
typedef PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(platform_get_all_files_of_type_begin);

#define PLATFORM_GET_ALL_FILES_OF_TYPE_END(name) void name(platform_file_group* FileGroup)
typedef PLATFORM_GET_ALL_FILES_OF_TYPE_END(platform_get_all_files_of_type_end);

#define PLATFORM_OPEN_FILE(name) platform_file_handle name(platform_file_group* FileGroup)
typedef PLATFORM_OPEN_FILE(platform_open_next_file);

#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle* Source, uint64 Offset, uint64 Size, void* Dest)
typedef PLATFORM_READ_DATA_FROM_FILE(platform_read_data_from_file);

#define PLATFORM_FILE_ERROR(name) void name(platform_file_handle* Handle, char* Message)
typedef PLATFORM_FILE_ERROR(platform_file_error);

#define PLATFORM_ALLOCATE_MEMORY(name) void* name(memory_index Size)
typedef PLATFORM_ALLOCATE_MEMORY(platform_allocate_memory);

#define PLATFORM_DEALLOCATE_MEMORY(name) void name(void* Memory)
typedef PLATFORM_DEALLOCATE_MEMORY(platform_deallocate_memory);

struct platform_api {
    platform_add_entry* AddEntry;
    platform_complete_all_work* CompleteAllWork;

    platform_get_all_files_of_type_begin* GetAllFilesOfTypeBegin;
    platform_get_all_files_of_type_end* GetAllFilesOfTypeEnd;
    platform_open_next_file* OpenNextFile;
    platform_read_data_from_file* ReadDataFromFile;
    platform_file_error* FileError;

    platform_allocate_memory* AllocateMemory;
    platform_deallocate_memory* DeallocateMemory;

    debug_platform_read_entire_file* DEBUGReadEntireFile;
    debug_platform_free_file_memory* DEBUGFreeFileMemory;
    debug_platform_write_entire_file* DEBUGWriteEntireFile;
};

struct game_memory {
    uint64 PermanentStorageSize;
    void* PermanentStorage; //* Required to be cleared to 0
    uint64 TransientStorageSize;
    void* TransientStorage; //* Required to be cleared to 0

    uint64 DebugStorageSize;
    void* DebugStorage; //* Required to be cleared to 0

    platform_work_queue* HighPriorityQueue;
    platform_work_queue* LowPriorityQueue;

    platform_api PlatformAPI;
};

#define BITMAP_BYTES_PER_PIXEL 4

struct game_offscreen_buffer {
    void* Memory;
    int Width;
    int Height;
    int Pitch;
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
    bool32 ExecutableReloaded;
    real32 dtForFrame;
    game_controller_input Controllers[5];
};

internal inline game_controller_input* GetController(game_input* Input, int ControllerIndex) {
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    return &Input->Controllers[ControllerIndex];
}

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory* Memory, game_input* Input, game_offscreen_buffer* Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory* Memory, game_sound_buffer* SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

global_variable platform_api Platform;

struct debug_record {
    char* Filename;
    char* BlockName;

    int32 Linenumber;
    uint32 Reserved;

    uint64 HitCount_CycleCount;
};

struct debug_event {
    uint64 Clock;
    uint16 ThreadIndex;
    uint16 CoreIndex;
    uint16 DebugRecordIndex;
    uint16 TranslationUnit;
    uint8 Type;
};

#define MAX_DEBUG_TRANSLATION_UNITS 2
#define MAX_DEBUG_EVENT_COUNT 16*65536
#define MAX_DEBUG_RECORD_COUNT 65536

struct debug_table {
    uint64 volatile EventArrayIndex_EventIndex;
    debug_event Events[2][MAX_DEBUG_EVENT_COUNT];

    uint32 RecordCount[MAX_DEBUG_TRANSLATION_UNITS];
    debug_record Records[MAX_DEBUG_TRANSLATION_UNITS][MAX_DEBUG_RECORD_COUNT];
};

extern debug_table* GlobalDebugTable;

#define DEBUG_GAME_FRAME_END(name) debug_table* name(game_memory* Memory)
typedef DEBUG_GAME_FRAME_END(debug_game_frame_end);

#define FRAME_MARKER() \
    {int Counter = __COUNTER__; \
    RecordDebugEvent(Counter, DebugEvent_FrameMarker); \
    debug_record* Record = GlobalDebugTable->Records[TRANSLATION_UNIT_INDEX] + Counter; \
    Record->Filename = __FILE__; \
    Record->Linenumber = __LINE__; \
    Record->BlockName = "FrameMarker";}

#define TIMED_BLOCK__(BlockName, Number, ...) timed_block TimedBlock##Number(__COUNTER__, __FILE__, __LINE__, BlockName, ##__VA_ARGS__);
#define TIMED_BLOCK_(BlockName, Number, ...) TIMED_BLOCK__(BlockName, Number, ##__VA_ARGS__);
#define TIMED_BLOCK(BlockName, ...) TIMED_BLOCK_(#BlockName, __LINE__, ##__VA_ARGS__);
#define TIMED_FUNCTION(...) TIMED_BLOCK_(__FUNCTION__, __LINE__ ##__VA_ARGS__);

#define BEGIN_BLOCK_(Counter, FilenameInit, LinenumberInit, BlockNameInit) \
        {debug_record* Record = GlobalDebugTable->Records[TRANSLATION_UNIT_INDEX] + Counter; \
        Record->Filename = FilenameInit; \
        Record->Linenumber = LinenumberInit; \
        Record->BlockName = BlockNameInit; \
        RecordDebugEvent(Counter, DebugEvent_BeginBlock);}

#define BEGIN_BLOCK(Name) \
    int Counter_##Name = __COUNTER__; \
    BEGIN_BLOCK_(Counter_##Name, __FILE__, __LINE__, #Name)

#define END_BLOCK_(Counter) \
    RecordDebugEvent(Counter, DebugEvent_EndBlock);

#define END_BLOCK(Name) END_BLOCK_(Counter_##Name)

enum debug_event_type {
    DebugEvent_FrameMarker,
    DebugEvent_BeginBlock,
    DebugEvent_EndBlock,
};

internal inline void RecordDebugEvent(uint32 RecordIndex, debug_event_type EventType) {
    uint64 ArrayIndex_EventIndex = AtomicAddU64(&GlobalDebugTable->EventArrayIndex_EventIndex, 1);
    uint32 EventIndex = ArrayIndex_EventIndex & 0xFFFFFFFF;
    Assert(EventIndex < MAX_DEBUG_EVENT_COUNT);
    debug_event* Event = GlobalDebugTable->Events[ArrayIndex_EventIndex >> 32] + EventIndex;
    Event->Clock = __rdtsc();
    Event->ThreadIndex = (uint16)GetThreadId();
    __rdtscp((uint32*)&Event->CoreIndex);
    Event->DebugRecordIndex = (uint16)RecordIndex;
    Event->TranslationUnit = TRANSLATION_UNIT_INDEX;
    Event->Type = (uint8)EventType;
}

struct timed_block {
    int32 Counter;

    timed_block(int32 CounterInit, char* Filename, int32 Linenumber, char* BlockName, int32 HitCountInit = 1) {
        Counter = CounterInit;
        BEGIN_BLOCK_(Counter, Filename, Linenumber, BlockName);
    }

    ~timed_block() {
        END_BLOCK_(Counter)
    }
};

struct debug_counter_snapshot {
    uint32 HitCount;
    uint64 CycleCount;
};

#define DEBUG_SNAPSHOT_COUNT 120
struct debug_counter_state {
    char* Filename;
    char* BlockName;

    int32 Linenumber;

    debug_counter_snapshot Snapshots[DEBUG_SNAPSHOT_COUNT];
};

struct debug_state {
    uint32 SnapshotIndex;
    uint32 CounterCount;
    debug_counter_state CounterStates[512];
};

internal void
UpdateDebugRecords(debug_state* DebugState, uint32 CounterCount, debug_record* Counters) {
    for (uint32 CounterIndex = 0; CounterIndex < CounterCount; CounterIndex++) {

        debug_record* Source = Counters + CounterIndex;
        debug_counter_state* Dest = DebugState->CounterStates + DebugState->CounterCount++;

        uint64 HitCount_CycleCount = AtomicExchangeU64((volatile uint64*)&Source->HitCount_CycleCount, 0);
        Dest->Filename = Source->Filename;
        Dest->BlockName = Source->BlockName;
        Dest->Linenumber = Source->Linenumber;
        Dest->Snapshots[DebugState->SnapshotIndex].HitCount = (uint32)(HitCount_CycleCount >> 32);
        Dest->Snapshots[DebugState->SnapshotIndex].CycleCount = (uint32)(HitCount_CycleCount & 0xFFFFFFFF);
    }
}

#endif
