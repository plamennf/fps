#include "pch.h"
#include "memory_arena.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#define is_power_of_two(x) ((x != 0) && ((x & (x - 1)) == 0))

inline uintptr_t align_forward(uintptr_t ptr, size_t alignment) {
    uintptr_t p, a, modulo;
    if (!is_power_of_two(alignment)) {
        assert(!"Alignment is not a power of two");
        return 0;
    }

    p = ptr;
    a = (uintptr_t)alignment;
    modulo = p & (a - 1);

    if (modulo) {
        p += a - modulo;
    }

    return p;
}

void Memory_Arena::init(size_t _size) {
#ifdef _WIN32
    base = VirtualAlloc(0, _size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    base = malloc(_size);
#endif
    size = _size;
    offset   = 0;
    commited = 0;
}

void Memory_Arena::init_from_other_arena(Memory_Arena *other, size_t _offset, size_t _size) {
    base = (void *)((u8 *)other->base + _offset);
    size = _size;
    offset   = 0;
    commited = 0;
}

void Memory_Arena::reset() {
    offset   = 0;
    commited = 0;
}

void *Memory_Arena::allocate_aligned(size_t _size, size_t alignment) {
    uintptr_t curr_ptr = (uintptr_t)base + (uintptr_t)offset;
    uintptr_t offs = align_forward(curr_ptr, alignment);
    offs -= (uintptr_t)base;

    if (offs + _size > size) {
        assert(!"Size is too large");
        return 0;
    }

    commited += _size;
    void *ptr = (void *)((uint8_t *)base + offs);
    offset = offs + _size;

    return ptr;
}

void *Memory_Arena::allocate(size_t _size) {
    if (!_size) {
        assert(!"Size is 0");
        return 0;
    }
    return allocate_aligned(_size, MEMORY_ARENA_DEFAULT_ALIGNMENT);
}
