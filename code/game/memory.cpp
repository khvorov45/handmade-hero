#if !defined(HANDMADE_MEMORY_CPP)
#define HANDMADE_MEMORY_CPP

#include "../types.h"
#include "../util.h"

struct memory_arena {
    memory_index Size;
    uint8* Base;
    memory_index Used;
};

internal void InitializeArena(memory_arena* Arena, memory_index Size, void* Base) {
    Arena->Size = Size;
    Arena->Base = (uint8*)Base;
    Arena->Used = 0;
}

#define PushStruct(Arena, type) (type*)PushSize(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type*)PushSize(Arena, (Count)*sizeof(type))

void* PushSize(memory_arena* Arena, memory_index Size) {
    Assert((Arena->Used + Size) <= Arena->Size);
    void* Result = Arena->Base + Arena->Used;
    Arena->Used += Size;
    return Result;
}

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))

inline void ZeroSize(memory_index Size, void* Ptr) {
    uint8* Byte = (uint8*)Ptr;
    while (Size--) {
        *Byte++ = 0;
    }
}

#endif
