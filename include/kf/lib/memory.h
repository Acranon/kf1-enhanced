#ifndef KF_MEMORY_H
#define KF_MEMORY_H

#include <kf/lib/enum.h>
#include <kf/lib/types.h>

#include <array>
#include <cstddef>
#include <memory>

enum class KfMemoryAllocationMode : s32 {
    KF_MEMORY_CREATE_ARENA = 0,
    KF_MEMORY_REBASE_ARENA = 1,
    KF_MEMORY_USE_HEAP = 2
}; using enum KfMemoryAllocationMode;

enum { KF_MEMORY_ALLOCATION_CAPACITY = 16 };

struct KfMemoryAllocation {
    std::unique_ptr<u8[]> heap;
    u8 *previous_cursor;
};

typedef struct KfMemoryAllocationState {
    u8 *cursor;
    std::size_t depth;
    std::array<KfMemoryAllocation, KF_MEMORY_ALLOCATION_CAPACITY> stack;
} KfMemoryAllocationState;

typedef struct KfMemoryArena {
    std::unique_ptr<u8[]> storage;
    u8 *start;
    u8 *end;
    KfMemoryAllocationState allocation;
} KfMemoryArena;

extern void *memory_allocate(KfMemoryArena &arena, std::size_t size);
extern void memory_allocation_reset(KfMemoryArena &arena);
extern void memory_destroy_arena(KfMemoryArena &arena);
extern void memory_set_allocation_mode(KfMemoryArena &arena, KfMemoryAllocationMode allocation_mode);
extern void memory_release_last(KfMemoryArena &arena);

#endif // KF_MEMORY_H
