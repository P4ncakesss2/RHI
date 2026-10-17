#include "handle_internal.h"

#ifndef HANDLE_MAX_POOLS
#define HANDLE_MAX_POOLS 256
#endif

typedef struct {
    void* poolPtr;
    HandlePoolShutdownFn shutdownFn;
    const char* name;
} HandleRegistryEntry;

static HandleRegistryEntry g_handleRegistry[HANDLE_MAX_POOLS];
static size_t g_handleRegistryCount = 0;

void HandlePoolRegistry_Add(void* pool, HandlePoolShutdownFn shutdownFn, const char* name) {
    if (g_handleRegistryCount >= HANDLE_MAX_POOLS) {
        fprintf(stderr, "[handle] Registry overflow: too many unique pools (%d max)\n", HANDLE_MAX_POOLS);
        assert(false);
        return;
    }
    for (size_t i = 0; i < g_handleRegistryCount; i++) {
        if (g_handleRegistry[i].poolPtr == pool) {
            return;
        }
    }
    g_handleRegistry[g_handleRegistryCount++] = (HandleRegistryEntry){
        .poolPtr = pool,
        .shutdownFn = shutdownFn,
        .name = name
    };
}

void HandlePoolRegistry_Remove(void* pool) {
    for (size_t i = 0; i < g_handleRegistryCount; i++) {
        if (g_handleRegistry[i].poolPtr == pool) {
            g_handleRegistry[i] = g_handleRegistry[--g_handleRegistryCount];
            return;
        }
    }
}

void HandlePools_ShutdownAll(void) {
    while (g_handleRegistryCount > 0) {
        size_t idx = g_handleRegistryCount - 1;
        HandleRegistryEntry entry = g_handleRegistry[idx];
        if (entry.shutdownFn && entry.poolPtr) {
            entry.shutdownFn(entry.poolPtr);
        }
    }
}