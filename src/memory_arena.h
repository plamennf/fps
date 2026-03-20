#pragma once

#define MEMORY_ARENA_DEFAULT_ALIGNMENT (2 * sizeof(void *))

struct Memory_Arena {
    void *base;
    size_t size;
    size_t offset;
    size_t commited;

    void init(size_t size);
    void init_from_other_arena(Memory_Arena *other, size_t offset, size_t size);
    
    void *allocate_aligned(size_t size, size_t alignment);
    void *allocate(size_t size);

    void reset();
    
    template <typename T>
    inline T *allocate_struct(size_t alignment = 4) {
        T *result = (T *)allocate_aligned(sizeof(T), alignment);
        return result;
    }

    template <typename T>
    inline T *allocate_array(size_t count, size_t alignment = 4) {
        T *result = (T *)allocate_aligned(count * sizeof(T), alignment);
        return result;
    }
};

