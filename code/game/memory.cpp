#if !defined(HANDMADE_MEMORY_CPP)
#define HANDMADE_MEMORY_CPP

#include "lib.hpp"
#include "../types.h"
#include "../util.h"

internal void InitializeArena(memory_arena* Arena, memory_index Size, void* Base) {
    Arena->Size = Size;
    Arena->Base = (uint8*)Base;
    Arena->Used = 0;
    Arena->TempCount = 0;
}

internal inline memory_index GetAlignmentOffset(memory_arena* Arena, memory_index Alignment) {
    memory_index ResultPointer = (memory_index)Arena->Base + Arena->Used;
    memory_index AlignmentOffset = 0;
    memory_index AlignmentMask = Alignment - 1;
    if ((memory_index)ResultPointer & AlignmentMask) {
        AlignmentOffset = Alignment - ((memory_index)ResultPointer & AlignmentMask);
    }
    return AlignmentOffset;
}

internal inline memory_index GetArenaSizeRemaining(memory_arena* Arena, memory_index Alignment = 4) {
    memory_index Result = Arena->Size - (Arena->Used + GetAlignmentOffset(Arena, Alignment));
    return Result;
}

#define PushStruct(Arena, type, ...) (type*)PushSize_(Arena, sizeof(type), ##__VA_ARGS__)
#define PushArray(Arena, Count, type, ...) (type*)PushSize_(Arena, (Count)*sizeof(type), ##__VA_ARGS__)
#define PushSize(Arena, Size, ...) PushSize_(Arena, Size, ##__VA_ARGS__)

void* PushSize_(memory_arena* Arena, memory_index SizeInit, memory_index Alignment = 4) {
    memory_index AlignmentOffset = GetAlignmentOffset(Arena, Alignment);
    memory_index Size = SizeInit + AlignmentOffset;
    Assert(Size >= SizeInit);
    Assert((Arena->Used + Size) <= Arena->Size);
    void* Result = Arena->Base + Arena->Used + AlignmentOffset;
    Arena->Used += Size;
    return Result;
}

inline temporary_memory BeginTemporaryMemory(memory_arena* Arena) {
    temporary_memory Result;
    Result.Arena = Arena;
    Result.Used = Arena->Used;
    ++Arena->TempCount;
    return Result;
}

inline void EndTemporaryMemory(temporary_memory TempMem) {
    memory_arena* Arena = TempMem.Arena;
    Assert(Arena->Used >= TempMem.Used);
    Arena->Used = TempMem.Used;
    Assert(Arena->TempCount > 0);
    --Arena->TempCount;
}

inline void CheckArena(memory_arena* Arena) {
    Assert(Arena->TempCount == 0);
}

internal inline void SubArena(memory_arena* Result, memory_arena* Arena, memory_index Size, memory_index Alignment = 16) {
    Result->Size = Size;
    Result->Base = (uint8*)PushSize(Arena, Size, Alignment);
    Result->TempCount = 0;
    Result->Used = 0;
}

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
#define ZeroArray(Count, Pointer) ZeroSize(Count * sizeof(Pointer[0]), Pointer)

inline void ZeroSize(memory_index Size, void* Ptr) {
    uint8* Byte = (uint8*)Ptr;
    while (Size--) {
        *Byte++ = 0;
    }
}

internal char* PushString(memory_arena* Arena, char* Source) {
    uint32 Size = 1;
    for (char* At = Source; *At; At++) { Size++; }
    char* Dest = (char*)PushSize_(Arena, Size);
    for (uint32 CharIndex = 0; CharIndex < Size; CharIndex++) {
        Dest[CharIndex] = Source[CharIndex];
    }
    return Dest;
}

internal void Copy(memory_index Size, void* SourceInit, void* DestInit) {
    uint8* Source = (uint8*)SourceInit;
    uint8* Dest = (uint8*)DestInit;
    while (Size--) { *Dest++ = *Source++; }
}

#endif
