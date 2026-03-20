#pragma once

#include "general.h"
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <eastl/vector.h>
#include <eastl/unordered_map.h>
#include <eastl/string.h>
#include <eastl/sort.h>
#include "memory_arena.h"

#define NS_PER_SECOND 1000000000.0

template <typename T>
inline T *copy_to_array(eastl::vector <T> const &v) {
    T *result = new T[v.size()];
    memcpy(result, v.data(), v.size() * sizeof(T));
    return result;
}
