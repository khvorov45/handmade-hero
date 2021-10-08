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

debug_record DebugRecordArray[];

struct timed_block {
    debug_record* Record;
    uint64 StartCycles;
    uint32 HitCount;

    timed_block(int32 Counter, char* Filename, int32 Linenumber, char* FunctionName, int32 HitCountInit = 1) {
        HitCount = HitCountInit;
        Assert(HitCount > 0);
        StartCycles = __rdtsc();
        Record = DebugRecordArray + Counter;
        Record->Filename = Filename;
        Record->Linenumber = Linenumber;
        Record->FunctionName = FunctionName;
    }

    ~timed_block() {
        Assert(HitCount > 0);
        uint64 Delta = (__rdtsc() - StartCycles) | ((uint64)HitCount << 32);
        AtomicAddU64((volatile uint64*)&Record->HitCount_CycleCount, Delta);
    }
};

#endif
