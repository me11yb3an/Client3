#pragma once
#include <stdint.h>
typedef struct {uint64_t live[3],peak[3],allocs[3],frees[3],failed,untracked,info_failures;} H7NativeMemory;
void h7_native_alloc(int id,unsigned type,unsigned requested);
void h7_native_snapshot(H7NativeMemory*out);
