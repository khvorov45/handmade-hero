#if !defined(HANDMADE_TYPES_H)
#define HANDMADE_TYPES_H

#include <stdint.h>
#include <float.h>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;

typedef size_t memory_index;

typedef int32 bool32;

typedef float real32;
typedef double real64;

#define Real32Maximum FLT_MAX
#define Real64Maximum DBL_MAX

#define internal static
#define local_persist static
#define global_variable static

#endif
