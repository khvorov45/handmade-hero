#if !defined(HANDMADE_DEBUG_CPP)
#define HANDMADE_DEBUG_CPP

#include "lib.hpp"
#include "../types.h"

#define TIMED_BLOCK timed_block TimedBlock##__LINE__(__COUNTER__, __FILE__, __LINE__, __FUNCTION__);

struct debug_record {};

debug_record DebugRecordArray[];

struct timed_block {
    uint64 StartCycleCount;
    uint32 ID;
    timed_block(int32 Counter, char* Filename, int32 Linenumber, char* FunctionName) {
        debug_record* Record = DebugRecordArray + Counter;
        //ID = FindIDFromFilenameLinenumber(Filename, Linenumber);
        //BEGIN_TIMED_BLOCK_(StartCycleCount);
    }
    ~timed_block() {
        //END_TIMED_BLOCK_(StartCycleCount, ID);
    }
};

#endif
