#ifndef HANDLE_H
#define HANDLE_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t Handle;

#define HANDLE_INVALID ((Handle)0xFFFFFFFFu)

static inline bool Handle_IsValid(Handle h) {
    return h != HANDLE_INVALID;
}

#define DEFINE_HANDLE_TYPE(Name)                                                          \
    typedef struct { Handle value; } Name;                                                \
    static inline Name Name##_Invalid(void) { Name h; h.value = HANDLE_INVALID; return h; } \
    static inline bool Name##_IsValid(Name h) { return Handle_IsValid(h.value); }         \
    static inline bool Name##_Equal(Name a, Name b) { return a.value == b.value; }

#endif