#if !defined(HANDMADE_DEBUG_CPP)
#define HANDMADE_DEBUG_CPP

#include "lib.hpp"
#include "../types.h"

#define TIMED_BLOCK__(Number, ...) timed_block TimedBlock##Number(__COUNTER__, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);
#define TIMED_BLOCK_(Number, ...) TIMED_BLOCK__(Number, ##__VA_ARGS__);
#define TIMED_BLOCK(...) TIMED_BLOCK_(__LINE__, ##__VA_ARGS__);

struct debug_record {
    char* Filename;
    char* FunctionName;

    int32 Linenumber;
    uint32 Reserved;

    uint64 HitCount_CycleCount;
};

enum debug_event_type {
    DebugEvent_BeginBlock,
    DebugEvent_EndBlock,
};

struct debug_event {
    uint64 Clock;
    uint16 ThreadIndex;
    uint16 CoreIndex;
    uint16 DebugRecordIndex;
    uint16 DebugRecordArrayIndex;
    uint8 Type;
};

debug_record DebugRecordArray[];

#define MAX_DEBUG_EVENT_COUNT 16*65536
debug_event GlobalDebugEventArray[2][MAX_DEBUG_EVENT_COUNT];
uint64 Global_DebugEventArrayIndex_DebugEventIndex;

internal inline void RecordDebugEvent(uint32 RecordIndex, debug_event_type EventType) {
    uint64 ArrayIndex_EventIndex = AtomicAddU64(&Global_DebugEventArrayIndex_DebugEventIndex, 1);
    uint32 EventIndex = ArrayIndex_EventIndex & 0xFFFFFFFF;
    Assert(EventIndex < MAX_DEBUG_EVENT_COUNT);
    debug_event* Event = GlobalDebugEventArray[ArrayIndex_EventIndex >> 32] + EventIndex;
    Event->Clock = __rdtsc();
    Event->ThreadIndex = (uint16)GetThreadId();
    __rdtscp((uint32*)&Event->CoreIndex);
    Event->DebugRecordIndex = (uint16)RecordIndex;
    Event->DebugRecordArrayIndex = 0;
    Event->Type = (uint8)EventType;
}

struct timed_block {
    debug_record* Record;
    uint64 StartCycles;
    uint32 HitCount;
    int32 Counter;

    timed_block(int32 CounterInit, char* Filename, int32 Linenumber, char* FunctionName, int32 HitCountInit = 1) {
        Counter = CounterInit;
        HitCount = HitCountInit;
        Assert(HitCount > 0);
        Record = DebugRecordArray + CounterInit;
        Record->Filename = Filename;
        Record->Linenumber = Linenumber;
        Record->FunctionName = FunctionName;

        StartCycles = __rdtsc();

        RecordDebugEvent(Counter, DebugEvent_BeginBlock);
    }

    ~timed_block() {
        Assert(HitCount > 0);
        uint64 Delta = (__rdtsc() - StartCycles) | ((uint64)HitCount << 32);
        AtomicAddU64((volatile uint64*)&Record->HitCount_CycleCount, Delta);

        RecordDebugEvent(Counter, DebugEvent_EndBlock);
    }
};

struct debug_counter_snapshot {
    uint32 HitCount;
    uint64 CycleCount;
};

#define DEBUG_SNAPSHOT_COUNT 120
struct debug_counter_state {
    char* Filename;
    char* FunctionName;

    int32 Linenumber;

    debug_counter_snapshot Snapshots[DEBUG_SNAPSHOT_COUNT];
};

struct debug_state {
    uint32 SnapshotIndex;
    uint32 CounterCount;
    debug_counter_state CounterStates[512];
    debug_game_frame_end_info FrameEndInfos[DEBUG_SNAPSHOT_COUNT];
};

internal void
UpdateDebugRecords(debug_state* DebugState, uint32 CounterCount, debug_record* Counters) {
    for (uint32 CounterIndex = 0; CounterIndex < CounterCount; CounterIndex++) {

        debug_record* Source = Counters + CounterIndex;
        debug_counter_state* Dest = DebugState->CounterStates + DebugState->CounterCount++;

        uint64 HitCount_CycleCount = AtomicExchangeU64((volatile uint64*)&Source->HitCount_CycleCount, 0);
        Dest->Filename = Source->Filename;
        Dest->FunctionName = Source->FunctionName;
        Dest->Linenumber = Source->Linenumber;
        Dest->Snapshots[DebugState->SnapshotIndex].HitCount = (uint32)(HitCount_CycleCount >> 32);
        Dest->Snapshots[DebugState->SnapshotIndex].CycleCount = (uint32)(HitCount_CycleCount & 0xFFFFFFFF);
    }
}

#endif
