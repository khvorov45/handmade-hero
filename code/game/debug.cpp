#if !defined(HANDMADE_DEBUG_CPP)
#define HANDMADE_DEBUG_CPP

#include "lib.hpp"
#include "../types.h"

#define TIMED_BLOCK__(Number, ...) timed_block TimedBlock##Number(__COUNTER__, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);
#define TIMED_BLOCK_(Number, ...) TIMED_BLOCK__(Number, ##__VA_ARGS__);
#define TIMED_BLOCK(...) TIMED_BLOCK_(__LINE__, ##__VA_ARGS__);

struct debug_record {
    uint64 Clocks;

    char* Filename;
    char* FunctionName;

    int32 Linenumber;
    uint32 HitCount;
};

debug_record DebugRecordArray[];

struct timed_block {
    debug_record* Record;
    timed_block(int32 Counter, char* Filename, int32 Linenumber, char* FunctionName, int32 HitCount = 1) {
        Record = DebugRecordArray + Counter;
        Record->Filename = Filename;
        Record->Linenumber = Linenumber;
        Record->FunctionName = FunctionName;
        Record->Clocks -= __rdtsc();
        Record->HitCount += HitCount;
    }
    ~timed_block() {
        Record->Clocks += __rdtsc();
    }
};

#endif
