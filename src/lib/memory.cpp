#include <kf/platform/prelude.h>
#include <kf/lib/memory.h>

#include <cstdint>
#include <cstdio>
#include <new>
#include <utility>

enum {
    MEMORY_INITIAL_ARENA_BYTES = 16 * 1024 * 1024,
    MEMORY_ALLOCATION_ALIGNMENT = alignof(std::max_align_t)
};

static void memory_forget_allocations(KfMemoryArena &arena)
{
    arena.allocation.stack = {};
    arena.allocation.depth = 0;
}

void memory_allocation_reset(KfMemoryArena &arena)
{
    memory_forget_allocations(arena);
    arena.allocation.cursor = arena.start;
}

void memory_destroy_arena(KfMemoryArena &arena)
{
    memory_forget_allocations(arena);
    arena = {};
}

void memory_set_allocation_mode(KfMemoryArena &arena, KfMemoryAllocationMode allocation_mode)
{
    switch (allocation_mode) {
    case KF_MEMORY_CREATE_ARENA:
        memory_destroy_arena(arena);
        arena.storage.reset(new (std::nothrow) u8[MEMORY_INITIAL_ARENA_BYTES]);
        if (!arena.storage) {
            kf::host_fail("Cannot allocate the resource arena.");
        }
        arena.start = arena.storage.get();
        arena.end = arena.storage.get() + MEMORY_INITIAL_ARENA_BYTES;
        memory_allocation_reset(arena);
        break;
    case KF_MEMORY_REBASE_ARENA:
        arena.start = arena.allocation.cursor;
        memory_allocation_reset(arena);
        break;
    case KF_MEMORY_USE_HEAP:
        arena.allocation.cursor = NULL;
        break;
    }
}

void *memory_allocate(KfMemoryArena &arena, std::size_t size)
{
    auto *allocation = &arena.allocation;
    if (!size || allocation->depth == KF_MEMORY_ALLOCATION_CAPACITY)
        return NULL;
    KfMemoryAllocation entry {};
    void *block;
    if (!allocation->cursor) {
        entry.heap.reset(new (std::nothrow) u8[size]);
        block = entry.heap.get();
        if (!block)
            return NULL;
    } else {
        const auto address = reinterpret_cast<std::uintptr_t>(allocation->cursor);
        const auto start = reinterpret_cast<std::uintptr_t>(arena.start);
        const auto end = reinterpret_cast<std::uintptr_t>(arena.end);
        if (address < start || address > end)
            return NULL;
        const auto padding = (MEMORY_ALLOCATION_ALIGNMENT - address % MEMORY_ALLOCATION_ALIGNMENT)
                             % MEMORY_ALLOCATION_ALIGNMENT;
        if (padding > end - address || size > end - address - padding)
            return NULL;
        entry.previous_cursor = allocation->cursor;
        block = allocation->cursor + padding;
        allocation->cursor += padding + size;
    }
    allocation->stack[allocation->depth++] = std::move(entry);
    return block;
}

void memory_release_last(KfMemoryArena &arena)
{
    auto *allocation = &arena.allocation;
    if (!allocation->depth)
        return;
    auto &entry = allocation->stack[--allocation->depth];
    if (!entry.heap)
        allocation->cursor = entry.previous_cursor;
    entry = {};
}
