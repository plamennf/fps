#pragma once

#include "general.h"
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
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

namespace glm {

    template<typename T>
    T normalize_or_zero(const T& v) {
        return glm::length2(v) > 0 ? glm::normalize(v) : T(0);
    }
    
}
