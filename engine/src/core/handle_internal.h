#ifndef HANDLE_H_INTERNAL
#define HANDLE_H_INTERNAL

#include "core/handle.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define HANDLE_INDEX_BITS 24
#define HANDLE_GEN_BITS   8
#define HANDLE_INDEX_MASK ((1u << HANDLE_INDEX_BITS) - 1)
#define HANDLE_GEN_MASK   ((1u << HANDLE_GEN_BITS) - 1)

static inline Handle Handle_Make(uint32_t index, uint32_t generation) {
    assert(index <= HANDLE_INDEX_MASK && "Handle index overflow - pool too large");
    return ((generation & HANDLE_GEN_MASK) << HANDLE_INDEX_BITS) | (index & HANDLE_INDEX_MASK);
}
static inline uint32_t Handle_Index(Handle h)      { return h & HANDLE_INDEX_MASK; }
static inline uint32_t Handle_Generation(Handle h) { return (h >> HANDLE_INDEX_BITS) & HANDLE_GEN_MASK; }

#ifndef NDEBUG
#define HANDLE_ASSERT(cond, fmt, ...)                                             \
    do {                                                                          \
        if (!(cond)) {                                                            \
            fprintf(stderr, "[handle] " fmt "\n", ##__VA_ARGS__);                 \
            assert(cond);                                                         \
        }                                                                         \
    } while (0)
#else
#define HANDLE_ASSERT(cond, fmt, ...) ((void)0)
#endif

typedef void (*HandlePoolShutdownFn)(void* pool);

void HandlePoolRegistry_Add(void* pool, HandlePoolShutdownFn shutdownFn, const char* name);
void HandlePoolRegistry_Remove(void* pool);
void HandlePools_ShutdownAll(void);

#define DEFINE_HANDLE_POOL(Name, PayloadType)                                                               \
                                                                                                              \
typedef struct {                                                                                            \
    PayloadType payload;                                                                                    \
    uint32_t    generation;                                                                                 \
    bool        alive;                                                                                      \
} Name##Slot;                                                                                                \
                                                                                                              \
typedef struct {                                                                                            \
    Name##Slot* slots;                                                                                      \
    uint32_t    slotCount;                                                                                  \
    uint32_t    slotCapacity;                                                                               \
    uint32_t*   freeList;                                                                                   \
    uint32_t    freeCount;                                                                                  \
    uint32_t    freeCapacity;                                                                               \
    uint32_t    liveCount;                                                                                  \
    bool        initialized;                                                                                \
    bool        shutdown;                                                                                   \
    bool        registered;                                                                                 \
    const char* debugName;                                                                                  \
} Name##Pool;                                                                                                \
                                                                                                              \
static inline void Name##Pool_Shutdown(Name##Pool* pool);                                                   \
                                                                                                              \
static inline void Name##Pool_InitCapacity(Name##Pool* pool, uint32_t initialCapacity) {                     \
    HANDLE_ASSERT(!pool->initialized, "%s: InitCapacity called twice / after first Alloc", #Name);            \
    memset(pool, 0, sizeof(*pool));                                                                          \
    pool->debugName = #Name;                                                                                 \
    pool->initialized = true;                                                                                \
    if (initialCapacity > 0) {                                                                               \
        pool->slots = (Name##Slot*)calloc(initialCapacity, sizeof(Name##Slot));                              \
        pool->slotCapacity = initialCapacity;                                                                \
        pool->freeList = (uint32_t*)malloc(initialCapacity * sizeof(uint32_t));                              \
        pool->freeCapacity = initialCapacity;                                                                \
    }                                                                                                          \
    HandlePoolRegistry_Add(pool, (HandlePoolShutdownFn)Name##Pool_Shutdown, #Name);                          \
    pool->registered = true;                                                                                 \
}                                                                                                               \
                                                                                                              \
static inline void Name##Pool_EnsureInit(Name##Pool* pool) {                                                  \
    HANDLE_ASSERT(!pool->shutdown,                                                                            \
        "%s: pool used after Shutdown - was a handle created during static teardown?", #Name);                 \
    if (!pool->initialized) {                                                                                  \
        pool->debugName = #Name;                                                                                \
        pool->initialized = true;                                                                               \
        HandlePoolRegistry_Add(pool, (HandlePoolShutdownFn)Name##Pool_Shutdown, #Name);                         \
        pool->registered = true;                                                                                \
    }                                                                                                              \
}                                                                                                                    \
                                                                                                              \
static inline void Name##Pool_Shutdown(Name##Pool* pool) {                                                      \
    if (pool->shutdown) return;                                                                                \
    HANDLE_ASSERT(!pool->shutdown, "%s: Shutdown called twice", #Name);                                          \
    HANDLE_ASSERT(pool->liveCount == 0,                                                                           \
        "%s: Shutdown called with %u handle(s) still alive (leak)", #Name, pool->liveCount);                      \
    if (pool->registered) {                                                                                   \
        HandlePoolRegistry_Remove(pool);                                                                     \
        pool->registered = false;                                                                            \
    }                                                                                                          \
    free(pool->slots);                                                                                             \
    free(pool->freeList);                                                                                           \
    memset(pool, 0, sizeof(*pool));                                                                                   \
    pool->shutdown = true;                                                                                             \
}                                                                                                                         \
                                                                                                              \
static inline Name Name##Pool_Alloc(Name##Pool* pool) {                                                        \
    Name##Pool_EnsureInit(pool);                                                                                 \
    uint32_t index;                                                                                              \
    if (pool->freeCount > 0) {                                                                                   \
        index = pool->freeList[--pool->freeCount];                                                               \
    } else {                                                                                                       \
        if (pool->slotCount == pool->slotCapacity) {                                                               \
            uint32_t newCap = pool->slotCapacity ? pool->slotCapacity * 2 : 16;                                     \
            pool->slots = (Name##Slot*)realloc(pool->slots, newCap * sizeof(Name##Slot));                            \
            memset(pool->slots + pool->slotCapacity, 0, (newCap - pool->slotCapacity) * sizeof(Name##Slot));          \
            pool->freeList = (uint32_t*)realloc(pool->freeList, newCap * sizeof(uint32_t));                            \
            pool->slotCapacity = newCap;                                                                                \
            pool->freeCapacity = newCap;                                                                                 \
        }                                                                                                                  \
        index = pool->slotCount++;                                                                                         \
    }                                                                                                                         \
    Name##Slot* slot = &pool->slots[index];                                                                                   \
    HANDLE_ASSERT(!slot->alive, "%s: internal error, allocated a live slot", #Name);                                           \
    slot->alive = true;                                                                                                          \
    memset(&slot->payload, 0, sizeof(PayloadType));                                                                                \
    pool->liveCount++;                                                                                                              \
    Name h; h.value = Handle_Make(index, slot->generation);                                                                           \
    return h;                                                                                                                          \
}                                                                                                                                         \
                                                                                                              \
static inline bool Name##Pool_IsAlive(Name##Pool* pool, Name handle) {                                                                    \
    if (!Handle_IsValid(handle.value)) return false;                                                                                        \
    uint32_t index = Handle_Index(handle.value);                                                                                              \
    if (index >= pool->slotCount) return false;                                                                                                \
    Name##Slot* slot = &pool->slots[index];                                                                                                      \
    return slot->alive && slot->generation == Handle_Generation(handle.value);                                                                    \
}                                                                                                                                                     \
                                                                                                              \
static inline PayloadType* Name##Pool_Resolve(Name##Pool* pool, Name handle) {                                         \
    if (!Handle_IsValid(handle.value)) return NULL;                                                                      \
    uint32_t index = Handle_Index(handle.value);                                                                         \
    if (index >= pool->slotCount) {                                                                                      \
        HANDLE_ASSERT(false, "%s: handle index %u out of range (slotCount=%u) - garbage or cross-type handle",           \
            #Name, index, pool->slotCount);                                                                              \
        return NULL;                                                                                                     \
    }                                                                                                                    \
    Name##Slot* slot = &pool->slots[index];                                                                              \
    if (!slot->alive) {                                                                                                  \
        HANDLE_ASSERT(false, "%s: use-after-free, handle index %u was destroyed", #Name, index);                         \
        return NULL;                                                                                                     \
    }                                                                                                                    \
    if (slot->generation != Handle_Generation(handle.value)) {                                                           \
        HANDLE_ASSERT(false, "%s: stale handle, index %u generation %u != current %u",                                   \
            #Name, index, Handle_Generation(handle.value), slot->generation);                                            \
        return NULL;                                                                                                     \
    }                                                                                                                    \
    return &slot->payload;                                                                                               \
}                                                                                                                        \
                                                                                                              \
static inline void Name##Pool_Free(Name##Pool* pool, Name handle) {                                                      \
    if (!Handle_IsValid(handle.value)) return;                                                                           \
    uint32_t index = Handle_Index(handle.value);                                                                         \
    HANDLE_ASSERT(index < pool->slotCount, "%s: Free called with out-of-range handle index %u", #Name, index);             \
    if (index >= pool->slotCount) return;                                                                                \
    Name##Slot* slot = &pool->slots[index];                                                                              \
    HANDLE_ASSERT(slot->alive, "%s: double free, handle index %u already destroyed", #Name, index);                      \
    if (!slot->alive) return;                                                                                            \
    HANDLE_ASSERT(slot->generation == Handle_Generation(handle.value), "%s: Free called with stale handle, index %u", #Name, index); \
    slot->alive = false;                                                                                                 \
    slot->generation = (slot->generation + 1) & HANDLE_GEN_MASK;                                                         \
    if (slot->generation == 0) {                                                                                         \
        fprintf(stderr, "[handle] %s: slot %u generation wrapped to 0, reuse collisions possible\n", #Name, index);        \
    }                                                                                                                    \
    if (pool->freeCount == pool->freeCapacity) {                                                                         \
        uint32_t newCap = pool->freeCapacity ? pool->freeCapacity * 2 : 16;                                               \
        pool->freeList = (uint32_t*)realloc(pool->freeList, newCap * sizeof(uint32_t));                                  \
        pool->freeCapacity = newCap;                                                                                     \
    }                                                                                                                    \
    pool->freeList[pool->freeCount++] = index;                                                                           \
    pool->liveCount--;                                                                                                   \
}                                                                                                                        \
                                                                                                              \
static inline uint32_t Name##Pool_Count(const Name##Pool* pool) { return pool->liveCount; }

#endif /* HANDLE_H */