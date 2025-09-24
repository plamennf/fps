#pragma once

#include "general.h"

#include <math.h>

const float PI = 3.14159265359f;
const float TAU = 6.28318530718f;

inline int absolute_value(int value) {
    if (value < 0) return -value;
    return value;
}

inline float absolute_value(float value) {
    float result = fabsf(value);
    return result;
}

inline float square_root(float value) {
    float result = _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(value)));
    return result;
}

inline s32 round_float32_to_s32(float value) {
    s32 result = _mm_cvtss_si32(_mm_set_ss(value));
    return result;
}

inline s32 floor_float32_to_s32(float value) {
    s32 result = _mm_cvtss_si32(_mm_floor_ss(_mm_setzero_ps(), _mm_set_ss(value)));
    return result;
}

inline float to_radians(float degrees) {
    float radians = degrees * (PI / 180.0f);
    return radians;
}

inline float to_degrees(float radians) {
    float degrees = radians * (180.0f / PI);
    return degrees;
}

inline float move_toward(float a, float b, float amount) {
    if (a > b) {
        a -= amount;
        if (a < b) a = b;
    } else {
        a += amount;
        if (a > b) a = b;
    }

    return a;
}

struct Vector2 {
    union {
        struct {
            float x;
            float y;
        };
        struct {
            float r;
            float g;
        };
        float e[2];
    };

    inline float &operator[](int index) {
        assert(index >= 0);
        assert(index < 2);
        return e[index];
    }

    inline float const &operator[](int index) const {
        assert(index >= 0);
        assert(index < 2);
        return e[index];
    }
};

#define v2 make_vector2

inline Vector2 make_vector2(float x, float y) {
    Vector2 result;

    result.x = x;
    result.y = y;

    return result;
}

inline Vector2 operator+(Vector2 a, Vector2 b) {
    Vector2 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;

    return result;
}

inline Vector2 operator-(Vector2 a, Vector2 b) {
    Vector2 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;

    return result;
}

inline Vector2 operator*(Vector2 a, float b) {
    Vector2 result;

    result.x = a.x * b;
    result.y = a.y * b;

    return result;
}

inline Vector2 operator*(float a, Vector2 b) {
    Vector2 result;

    result.x = a * b.x;
    result.y = a * b.y;

    return result;
}

inline Vector2 operator/(Vector2 a, float b) {
    Vector2 result;

    float inv_b = 1.0f / b;

    result.x = a.x * inv_b;
    result.y = a.y * inv_b;

    return result;
}

inline Vector2 &operator+=(Vector2 &a, Vector2 b) {
    a.x += b.x;
    a.y += b.y;
    return a;
}

inline Vector2 &operator-=(Vector2 &a, Vector2 b) {
    a.x -= b.x;
    a.y -= b.y;
    return a;
}

inline Vector2 &operator*=(Vector2 &a, float b) {
    a.x *= b;
    a.y *= b;
    return a;
}

inline bool operator<(Vector2 a, Vector2 b) {
    return a.x < b.x && a.y < b.y;
}

inline bool operator>(Vector2 a, Vector2 b) {
    return a.x > b.x && a.y > b.y;
}

inline float length_squared(Vector2 v) {
    return v.x*v.x + v.y*v.y;
}

inline float length(Vector2 v) {
    return sqrtf(length_squared(v));
}
    
inline Vector2 normalize(Vector2 v) {
    float multiplier = 1.0f / length(v);
    v.x *= multiplier;
    v.y *= multiplier;
    return v;
}

inline Vector2 normalize_or_zero(Vector2 v) {
    Vector2 result = {};
    
    float len_sq = length_squared(v);
    if (len_sq > 0.0001f * 0.0001f) {
        float multiplier = 1.0f / sqrtf(len_sq);
        result.x = v.x * multiplier;
        result.y = v.y * multiplier;
    }

    return result;
}

inline Vector2 componentwise_product(Vector2 a, Vector2 b) {
    Vector2 result;

    result.x = a.x * b.x;
    result.y = a.y * b.y;

    return result;
}

inline float dot_product(Vector2 a, Vector2 b) {
    return a.x*b.x + a.y*b.y;
}

inline Vector2 get_vec2(float theta) {
    float ct = cosf(theta);
    float st = sinf(theta);

    return make_vector2(ct, st);
}

inline Vector2 absolute_value(Vector2 v) {
    Vector2 result;

    result.x = fabsf(v.x);
    result.y = fabsf(v.y);

    return result;
}

inline Vector2 rotate(Vector2 v, float theta) {
    float ct = cosf(theta);
    float st = sinf(theta);

    Vector2 result;

    result.x = v.x*ct - v.y*st;
    result.y = v.x*st + v.y*ct;

    return result;
}

inline Vector2 move_toward(Vector2 a, Vector2 b, float amount) {
    Vector2 result;

    result.x = move_toward(a.x, b.x, amount);
    result.y = move_toward(a.y, b.y, amount);

    return result;
}

inline Vector2 minv(Vector2 a, Vector2 b) {
    Vector2 result;

    result.x = Min(a.x, b.x);
    result.y = Min(a.y, b.y);

    return result;
}

inline Vector2 maxv(Vector2 a, Vector2 b) {
    Vector2 result;

    result.x = Max(a.x, b.x);
    result.y = Max(a.y, b.y);

    return result;
}

inline Vector2 clampv(Vector2 v, Vector2 a, Vector2 b) {
    return maxv(a, minv(a, b));
}

struct Vector3 {
    union {
        struct {
            float x;
            float y;
            float z;
        };
        struct {
            float r;
            float g;
            float b;
        };
        float e[3];
    };

    inline float &operator[](int index) {
        assert(index >= 0);
        assert(index < 3);
        return e[index];
    }

    inline float const &operator[](int index) const {
        assert(index >= 0);
        assert(index < 3);
        return e[index];
    }
};

#define v3 make_vector3

inline Vector3 make_vector3(float x, float y, float z) {
    Vector3 result;

    result.x = x;
    result.y = y;
    result.z = z;

    return result;
}

inline Vector3 make_vector3(Vector2 xy, float z) {
    Vector3 result;

    result.x = xy.x;
    result.y = xy.y;
    result.z = z;

    return result;
}

inline Vector3 operator-(Vector3 a) {
    Vector3 result;

    result.x = -a.x;
    result.y = -a.y;
    result.z = -a.z;

    return result;
}

inline Vector3 operator+(Vector3 a, Vector3 b) {
    Vector3 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;

    return result;
}

inline Vector3 operator-(Vector3 a, Vector3 b) {
    Vector3 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;

    return result;
}

inline Vector3 operator*(Vector3 a, float b) {
    Vector3 result;

    result.x = a.x * b;
    result.y = a.y * b;
    result.z = a.z * b;

    return result;
}

inline Vector3 operator*(float a, Vector3 b) {
    Vector3 result;

    result.x = a * b.x;
    result.y = a * b.y;
    result.z = a * b.z;

    return result;
}

inline Vector3 operator/(Vector3 a, float b) {
    Vector3 result;

    float inv_b = 1.0f / b;

    result.x = a.x * inv_b;
    result.y = a.y * inv_b;
    result.z = a.z * inv_b;

    return result;
}

inline Vector3 &operator+=(Vector3 &a, Vector3 b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

inline Vector3 &operator-=(Vector3 &a, Vector3 b) {
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}

inline Vector3 &operator/=(Vector3 &a, float b) {
    float inv_b = 1.0f / b;
    a.x *= inv_b;
    a.y *= inv_b;
    a.z *= inv_b;
    return a;
}

inline bool operator<(Vector3 a, Vector3 b) {
    return a.x < b.x && a.y < b.y && a.z < b.z;
}

inline bool operator>(Vector3 a, Vector3 b) {
    return a.x > b.x && a.y > b.y && a.z > b.z;
}

inline float length_squared(Vector3 v) {
    return v.x*v.x + v.y*v.y + v.z*v.z;
}

inline float length(Vector3 v) {
    return sqrtf(length_squared(v));
}
    
inline Vector3 normalize(Vector3 v) {
    float multiplier = 1.0f / length(v);
    v.x *= multiplier;
    v.y *= multiplier;
    v.z *= multiplier;
    return v;
}

inline Vector3 normalize_or_zero(Vector3 v) {
    Vector3 result = {};
    
    float len_sq = length_squared(v);
    if (len_sq > 0.0001f * 0.0001f) {
        float multiplier = 1.0f / sqrtf(len_sq);
        result.x = v.x * multiplier;
        result.y = v.y * multiplier;
        result.z = v.z * multiplier;
    }

    return result;
}

inline Vector3 cross_product(Vector3 a, Vector3 b) {
    Vector3 result;
    result.x = a.y*b.z - a.z*b.y;
    result.y = a.z*b.x - a.x*b.z;
    result.z = a.x*b.y - a.y*b.x;
    return result;
}

inline float get_barycentric(Vector3 p0, Vector3 p1, Vector3 p2, Vector2 pos) {
    float det = (p1.z - p2.z) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.z - p2.z);
    float l0 = ((p1.z - p2.z) * (pos.x - p2.x) + (p2.x - p1.x) * (pos.y - p2.z)) / det;
    float l1 = ((p2.z - p0.z) * (pos.x - p2.x) + (p0.x - p2.x) * (pos.y - p2.z)) / det;
    float l2 = 1.0f - l0 - l1;
    float result = l0*p0.y + l1*p1.y + l2*p2.y;
    return result;
}

inline Vector3 componentwise_product(Vector3 a, Vector3 b) {
    Vector3 result;

    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;

    return result;
}

inline float distance(Vector3 a, Vector3 b) {
    return sqrtf(Square(a.x-b.x) + Square(a.y-b.y) + Square(a.z-b.z));
}

inline Vector3 lerp(Vector3 a, Vector3 b, float t) {
    return a + t * (b - a);
}

struct Vector4 {
    union {
        struct {
            float x;
            float y;
            float z;
            float w;
        };
        struct {
            float r;
            float g;
            float b;
            float a;
        };
        struct {
            Vector3 xyz;
            float _unused1;
        };
        float e[4];
    };

    inline float &operator[](int index) {
        assert(index >= 0);
        assert(index < 4);
        return e[index];
    }

    inline float const &operator[](int index) const {
        assert(index >= 0);
        assert(index < 4);
        return e[index];
    }
};

#define v4 make_vector4

inline Vector4 make_vector4(float x, float y, float z, float w) {
    Vector4 result;

    result.x = x;
    result.y = y;
    result.z = z;
    result.w = w;

    return result;
}

inline Vector4 make_vector4(Vector3 xyz, float w) {
    Vector4 result;

    result.x = xyz.x;
    result.y = xyz.y;
    result.z = xyz.z;
    result.w = w;

    return result;
}

inline Vector4 operator+(Vector4 a, Vector4 b) {
    Vector4 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    result.w = a.w + b.w;

    return result;
}

inline Vector4 operator-(Vector4 a, Vector4 b) {
    Vector4 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    result.w = a.w - b.w;

    return result;
}

inline Vector4 operator*(Vector4 a, float b) {
    Vector4 result;

    result.x = a.x * b;
    result.y = a.y * b;
    result.z = a.z * b;
    result.w = a.w * b;

    return result;    
}

inline Vector4 operator*(float a, Vector4 b) {
    Vector4 result;

    result.x = a * b.x;
    result.y = a * b.y;
    result.z = a * b.z;
    result.w = a * b.w;

    return result;    
}

inline Vector4 operator/(Vector4 a, float b) {
    Vector4 result;

    float inv_b = 1.0f / b;

    result.x = a.x * inv_b;
    result.y = a.y * inv_b;
    result.z = a.z * inv_b;
    result.w = a.w * inv_b;

    return result;
}

inline Vector4 lerp(Vector4 a, Vector4 b, float t) {
    return a + t * (b - a);
}

inline u32 argb_color(Vector4 color) {
    u32 ir = (u32)(color.x * 255.0f);
    u32 ig = (u32)(color.y * 255.0f);
    u32 ib = (u32)(color.z * 255.0f);
    u32 ia = (u32)(color.w * 255.0f);

    return (ia << 24) | (ir << 16) | (ig << 8) | (ib << 0);
}

inline float length_squared(Vector4 v) {
    return v.x*v.x + v.y*v.y + v.z*v.z + v.w*v.w;
}

inline float length(Vector4 v) {
    return sqrtf(length_squared(v));
}

inline Vector4 normalize_or_zero(Vector4 v) {
    Vector4 result = {};
    
    float len_sq = length_squared(v);
    if (len_sq > 0.0001f * 0.0001f) {
        float multiplier = 1.0f / sqrtf(len_sq);
        result.x = v.x * multiplier;
        result.y = v.y * multiplier;
        result.z = v.z * multiplier;
        result.w = v.w * multiplier;
    }

    return result;
}


struct Vector2i {
    union {
        struct {
            int x;
            int y;
        };
        int e[2];
    };
    
    inline int &operator[](int index) {
        assert(index >= 0);
        assert(index < 2);
        return e[index];
    }

    inline int const &operator[](int index) const {
        assert(index >= 0);
        assert(index < 2);
        return e[index];
    }
};

#define make_vector2i v2i

inline Vector2i make_vector2i(int x, int y) {
    Vector2i result;

    result.x = x;
    result.y = y;

    return result;
}

inline Vector2 to_vec2(Vector2i v) {
    Vector2 result;

    result.x = (float)v.x;
    result.y = (float)v.y;

    return result;
}

struct Matrix4 {
    union {
        struct {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float e[4][4];
        float le[16];
    };
};

inline Matrix4 matrix4_identity() {
    Matrix4 result = {};

    result._11 = 1.0f;
    result._22 = 1.0f;
    result._33 = 1.0f;
    result._44 = 1.0f;

    return result;
}

inline Vector4 operator*(Matrix4 m, Vector4 v) {
    Vector4 result;
    result.x = m._11 * v.x + m._12 * v.y + m._13 * v.z + m._14 * v.w;
    result.y = m._21 * v.x + m._22 * v.y + m._23 * v.z + m._24 * v.w;
    result.z = m._31 * v.x + m._32 * v.y + m._33 * v.z + m._34 * v.w;
    result.w = m._41 * v.x + m._42 * v.y + m._43 * v.z + m._44 * v.w;
    return result;
}

inline Matrix4 operator*(Matrix4 a, Matrix4 b) {
    Matrix4 result;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            float sum = 0.0f;
            for (int e = 0; e < 4; e++) {
                sum += a.e[row][e] * b.e[e][col];
            }
            result.e[row][col] = sum;
        }
    }
    return result;
}

inline Matrix4 transpose(Matrix4 m) {
    Matrix4 result;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            result.e[row][col] = m.e[col][row];
        }
    }
    return result;
}

inline Matrix4 inverse(Matrix4 m) {
    Matrix4 inv;
    float det;
    int i;

    inv.le[0] = m.le[5]  * m.le[10] * m.le[15] - 
             m.le[5]  * m.le[11] * m.le[14] - 
             m.le[9]  * m.le[6]  * m.le[15] + 
             m.le[9]  * m.le[7]  * m.le[14] +
             m.le[13] * m.le[6]  * m.le[11] - 
             m.le[13] * m.le[7]  * m.le[10];

    inv.le[4] = -m.le[4]  * m.le[10] * m.le[15] + 
              m.le[4]  * m.le[11] * m.le[14] + 
              m.le[8]  * m.le[6]  * m.le[15] - 
              m.le[8]  * m.le[7]  * m.le[14] - 
              m.le[12] * m.le[6]  * m.le[11] + 
              m.le[12] * m.le[7]  * m.le[10];

    inv.le[8] = m.le[4]  * m.le[9] * m.le[15] - 
             m.le[4]  * m.le[11] * m.le[13] - 
             m.le[8]  * m.le[5] * m.le[15] + 
             m.le[8]  * m.le[7] * m.le[13] + 
             m.le[12] * m.le[5] * m.le[11] - 
             m.le[12] * m.le[7] * m.le[9];

    inv.le[12] = -m.le[4]  * m.le[9] * m.le[14] + 
               m.le[4]  * m.le[10] * m.le[13] +
               m.le[8]  * m.le[5] * m.le[14] - 
               m.le[8]  * m.le[6] * m.le[13] - 
               m.le[12] * m.le[5] * m.le[10] + 
               m.le[12] * m.le[6] * m.le[9];

    inv.le[1] = -m.le[1]  * m.le[10] * m.le[15] + 
              m.le[1]  * m.le[11] * m.le[14] + 
              m.le[9]  * m.le[2] * m.le[15] - 
              m.le[9]  * m.le[3] * m.le[14] - 
              m.le[13] * m.le[2] * m.le[11] + 
              m.le[13] * m.le[3] * m.le[10];

    inv.le[5] = m.le[0]  * m.le[10] * m.le[15] - 
             m.le[0]  * m.le[11] * m.le[14] - 
             m.le[8]  * m.le[2] * m.le[15] + 
             m.le[8]  * m.le[3] * m.le[14] + 
             m.le[12] * m.le[2] * m.le[11] - 
             m.le[12] * m.le[3] * m.le[10];

    inv.le[9] = -m.le[0]  * m.le[9] * m.le[15] + 
              m.le[0]  * m.le[11] * m.le[13] + 
              m.le[8]  * m.le[1] * m.le[15] - 
              m.le[8]  * m.le[3] * m.le[13] - 
              m.le[12] * m.le[1] * m.le[11] + 
              m.le[12] * m.le[3] * m.le[9];

    inv.le[13] = m.le[0]  * m.le[9] * m.le[14] - 
              m.le[0]  * m.le[10] * m.le[13] - 
              m.le[8]  * m.le[1] * m.le[14] + 
              m.le[8]  * m.le[2] * m.le[13] + 
              m.le[12] * m.le[1] * m.le[10] - 
              m.le[12] * m.le[2] * m.le[9];

    inv.le[2] = m.le[1]  * m.le[6] * m.le[15] - 
             m.le[1]  * m.le[7] * m.le[14] - 
             m.le[5]  * m.le[2] * m.le[15] + 
             m.le[5]  * m.le[3] * m.le[14] + 
             m.le[13] * m.le[2] * m.le[7] - 
             m.le[13] * m.le[3] * m.le[6];

    inv.le[6] = -m.le[0]  * m.le[6] * m.le[15] + 
              m.le[0]  * m.le[7] * m.le[14] + 
              m.le[4]  * m.le[2] * m.le[15] - 
              m.le[4]  * m.le[3] * m.le[14] - 
              m.le[12] * m.le[2] * m.le[7] + 
              m.le[12] * m.le[3] * m.le[6];

    inv.le[10] = m.le[0]  * m.le[5] * m.le[15] - 
              m.le[0]  * m.le[7] * m.le[13] - 
              m.le[4]  * m.le[1] * m.le[15] + 
              m.le[4]  * m.le[3] * m.le[13] + 
              m.le[12] * m.le[1] * m.le[7] - 
              m.le[12] * m.le[3] * m.le[5];

    inv.le[14] = -m.le[0]  * m.le[5] * m.le[14] + 
               m.le[0]  * m.le[6] * m.le[13] + 
               m.le[4]  * m.le[1] * m.le[14] - 
               m.le[4]  * m.le[2] * m.le[13] - 
               m.le[12] * m.le[1] * m.le[6] + 
               m.le[12] * m.le[2] * m.le[5];

    inv.le[3] = -m.le[1] * m.le[6] * m.le[11] + 
              m.le[1] * m.le[7] * m.le[10] + 
              m.le[5] * m.le[2] * m.le[11] - 
              m.le[5] * m.le[3] * m.le[10] - 
              m.le[9] * m.le[2] * m.le[7] + 
              m.le[9] * m.le[3] * m.le[6];

    inv.le[7] = m.le[0] * m.le[6] * m.le[11] - 
             m.le[0] * m.le[7] * m.le[10] - 
             m.le[4] * m.le[2] * m.le[11] + 
             m.le[4] * m.le[3] * m.le[10] + 
             m.le[8] * m.le[2] * m.le[7] - 
             m.le[8] * m.le[3] * m.le[6];

    inv.le[11] = -m.le[0] * m.le[5] * m.le[11] + 
               m.le[0] * m.le[7] * m.le[9] + 
               m.le[4] * m.le[1] * m.le[11] - 
               m.le[4] * m.le[3] * m.le[9] - 
               m.le[8] * m.le[1] * m.le[7] + 
               m.le[8] * m.le[3] * m.le[5];

    inv.le[15] = m.le[0] * m.le[5] * m.le[10] - 
              m.le[0] * m.le[6] * m.le[9] - 
              m.le[4] * m.le[1] * m.le[10] + 
              m.le[4] * m.le[2] * m.le[9] + 
              m.le[8] * m.le[1] * m.le[6] - 
              m.le[8] * m.le[2] * m.le[5];

    det = m.le[0] * inv.le[0] + m.le[1] * inv.le[4] + m.le[2] * inv.le[8] + m.le[3] * inv.le[12];

    if (det == 0)
        return inv;

    det = 1.0f / det;

    for (i = 0; i < 16; i++)
        inv.le[i] = inv.le[i] * det;

    return inv;
}

inline Matrix4 make_perspective(float aspect_ratio, float fov_in_degrees, float z_near, float z_far) {
    float y_scale = (1.0f / tanf(fov_in_degrees * 0.5f * (PI / 180.0f))) * aspect_ratio;
    float x_scale = y_scale / aspect_ratio;
    float frustum_length = z_far - z_near;

    Matrix4 m = {};
    m._11 = x_scale;
    m._22 = y_scale;
#if 1
    m._33 = -((z_far + z_near) / frustum_length);
    m._34 = -((2 * z_near * z_far) / frustum_length);
#else
    m._33 = ((z_far + z_near) / frustum_length);
    m._34 = ((2 * z_near * z_far) / frustum_length);
#endif
    m._43 = -1.0f;
    m._44 = 0.0f;
    return m;
}

inline Matrix4 make_x_rotation(float t) {
    auto m = matrix4_identity();

    float ct = cosf(t);
    float st = sinf(t);

    m._22 = ct;
    m._23 = -st;
    m._32 = st;
    m._33 = ct;

    return m;
}

inline Matrix4 make_y_rotation(float t) {
    auto m = matrix4_identity();

    float ct = cosf(t);
    float st = sinf(t);

    m._11 = ct;
    m._31 = -st;
    m._13 = st;
    m._33 = ct;

    return m;
}

inline Matrix4 make_z_rotation(float t) {
    auto m = matrix4_identity();

    float ct = cosf(t);
    float st = sinf(t);

    m._11 = ct;
    m._12 = -st;
    m._21 = st;
    m._22 = ct;

    return m;
}

inline Matrix4 make_look_at_matrix(Vector3 pos, Vector3 target, Vector3 world_up) {
    Vector3 z_axis = normalize_or_zero(pos - target);
    Vector3 x_axis = normalize_or_zero(cross_product(normalize_or_zero(world_up), z_axis));
    Vector3 y_axis = cross_product(z_axis, x_axis);

    Matrix4 translation = matrix4_identity();
    translation._14 = -pos.x;
    translation._24 = -pos.y;
    translation._34 = -pos.z;

    Matrix4 rotation = matrix4_identity();
    rotation._11 = x_axis.x;
    rotation._12 = x_axis.y;
    rotation._13 = x_axis.z;
    rotation._21 = y_axis.x;
    rotation._22 = y_axis.y;
    rotation._23 = y_axis.z;
    rotation._31 = z_axis.x;
    rotation._32 = z_axis.y;
    rotation._33 = z_axis.z;

    return rotation * translation;
}

inline Matrix4 make_transformation_matrix(Vector3 position, Vector3 rotation, Vector3 scale) {
    Matrix4 m = matrix4_identity();

    m._14 = position.x;
    m._24 = position.y;
    m._34 = position.z;

    m._11 = scale.x;
    m._22 = scale.y;
    m._33 = scale.z;

    Matrix4 rx = make_x_rotation(rotation.x * (PI / 180.0f));
    Matrix4 ry = make_y_rotation(rotation.y * (PI / 180.0f));
    Matrix4 rz = make_z_rotation(rotation.z * (PI / 180.0f));

    return m * rx * ry * rz;
}

inline Matrix4 make_transformation_matrix(Vector3 position, Vector3 rotation, float scale) {
    return make_transformation_matrix(position, rotation, make_vector3(scale, scale, scale));
}

inline Matrix4 make_orthographic(float l, float r, float b, float t, float n, float f) {
    Matrix4 result = matrix4_identity();

    result._11 = 2.0f/(r-l);
    result._22 = 2.0f/(t-b);
    result._33 = -2.0f/(f-n);
    result._14 = -((r+l)/(r-l));
    result._24 = -((t+b)/(t-b));
    result._34 = -((f+n)/(f-n));
    
    return result;
}

struct Quaternion {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float w = 1.0f;
};

inline Quaternion operator*(Quaternion a, Quaternion b) {
    Quaternion result = {};
        
    result.w = (a.w * b.w) - (a.x * b.x) - (a.y * b.y) - (a.z * b.z);
    result.x = (a.x * b.w) + (a.w * b.x) + (a.y * b.z) - (a.z * b.y);
    result.y = (a.y * b.w) + (a.w * b.y) + (a.z * b.x) - (a.x * b.z);
    result.z = (a.z * b.w) + (a.w * b.z) + (a.x * b.y) - (a.y * b.x);

    return result;
}

inline Quaternion operator*(Quaternion a, Vector3 b) {
    Quaternion result = {};
        
    result.w = - (a.x * b.x) - (a.y * b.y) - (a.z * b.z);
    result.x =   (a.w * b.x) + (a.y * b.z) - (a.z * b.y);
    result.y =   (a.w * b.y) + (a.z * b.x) - (a.x * b.z);
    result.z =   (a.w * b.z) + (a.x * b.y) - (a.y * b.x);
        
    return result;
}

inline void set_from_axis_and_angle(Quaternion *q, Vector3 v, float angle) {
    float half_angle_radians = (angle * 0.5f) * (PI / 180.0f);

    float sine_half_angle = sinf(half_angle_radians);
    float cosine_half_angle = cosf(half_angle_radians);

    q->x = v.x * sine_half_angle;
    q->y = v.y * sine_half_angle;
    q->z = v.z * sine_half_angle;
    q->w = cosine_half_angle;
}

inline Quaternion conjugate(Quaternion q) {
    Quaternion result;

    result.x = -q.x;
    result.y = -q.y;
    result.z = -q.z;
    result.w = q.w;
        
    return result;
}

inline Matrix4 get_rotation_matrix(Quaternion q) {
    Matrix4 m = {};

    float xy = q.x*q.y;
    float xz = q.x*q.z;
    float xw = q.x*q.w;
    float yz = q.y*q.z;
    float yw = q.y*q.w;
    float zw = q.z*q.w;
    float x_sq = q.x*q.x;
    float y_sq = q.y*q.y;
    float z_sq = q.z*q.z;
        
    m._11 = 1 - 2 * (y_sq + z_sq);
    m._21 = 2 * (xy - zw);
    m._31 = 2 * (xz + yw);
    m._41 = 0.0f;

    m._12 = 2 * (xy + zw);
    m._22 = 1.0f - 2.0f * (x_sq + z_sq);
    m._32 = 2 * (yz - xw);
    m._42 = 0.0f;

    m._13 = 2 * (xz - yw);
    m._23 = 2 * (yz + xw);
    m._33 = 1.0f - 2.0f * (x_sq + y_sq);
    m._43 = 0.0f;

    m._14 = 0.0f;
    m._24 = 0.0f;
    m._34 = 0.0f;
    m._44 = 1.0f;

    return m;
}

inline float length_squared(Quaternion q) {
    return q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
}

inline float length(Quaternion q) {
    return sqrtf(length_squared(q));
}

inline Quaternion normalize_or_zero(Quaternion q) {
    Quaternion result = {};
    
    float len_sq = length_squared(q);
    if (len_sq < 0.001f * 0.001f) result;
    float inv_len = 1.0f / sqrtf(len_sq);

    result.x = q.x * inv_len;
    result.y = q.y * inv_len;
    result.z = q.z * inv_len;
    result.w = q.w * inv_len;

    return result;
}

struct Rectangle2i {
    int x;
    int y;
    int width;
    int height;
};

inline Rectangle2i aspect_ratio_fit(int window_width, int window_height, int render_width, int render_height) {
    Rectangle2i result = {};
    if (!window_width || !window_height || !render_width || !render_height) return result;

    float optimal_window_width = (float)window_height * ((float)render_width / (float)render_height);
    float optimal_window_height = (float)window_width * ((float)render_height / (float)render_width);

    if ((float)window_width > optimal_window_width) {
        result.y = 0;
        result.height = (int)window_height;

        result.width = (int)optimal_window_width;
        result.x = (window_width - result.width) / 2;
    } else {
        result.x = 0;
        result.width = (int)window_width;

        result.height = (int)optimal_window_height;
        result.y = (window_height - result.height) / 2;
    }

    return result;
}

struct Rectangle2 {
    float x;
    float y;
    float width;
    float height;
};

inline bool is_touching_left(Rectangle2 a, Rectangle2 b, Vector2 vel) {
    float al = a.x;
    float ar = al + a.width;
    float ab = a.y;
    float at = a.y + a.height;
    
    float bl = b.x;
    float br = bl + b.width;
    float bb = b.y;
    float bt = b.y + b.height;

    return ar + vel.x > bl &&
        al < bl &&
        ab < bt &&
        at > bb;
}

inline bool is_touching_right(Rectangle2 a, Rectangle2 b, Vector2 vel) {
    float al = a.x;
    float ar = al + a.width;
    float ab = a.y;
    float at = a.y + a.height;

    float bl = b.x;
    float br = bl + b.width;
    float bb = b.y;
    float bt = b.y + b.height;

    return al + vel.x < br &&
        ar > br &&
        ab < bt &&
        at > bb;
}

inline bool is_touching_top(Rectangle2 a, Rectangle2 b, Vector2 vel) {
    float al = a.x;
    float ar = al + a.width;
    float ab = a.y;
    float at = a.y + a.height;

    float bl = b.x;
    float br = bl + b.width;
    float bb = b.y;
    float bt = b.y + b.height;

    return ab + vel.y < bt &&
        at > bt &&
        ar > bl &&
        al < br;
}

inline bool is_touching_bottom(Rectangle2 a, Rectangle2 b, Vector2 vel) {
    float al = a.x;
    float ar = al + a.width;
    float ab = a.y;
    float at = a.y + a.height;

    float bl = b.x;
    float br = bl + b.width;
    float bb = b.y;
    float bt = b.y + b.height;

    return at + vel.y > bb &&
        ab < bb &&
        ar > bl &&
        al < br;
}

inline bool are_intersecting(Rectangle2 a, Rectangle2 b) {
#if 0
    // @TODO: Speed.
    bool result = (is_touching_left(a, b, v2(0, 0)) ||
                   is_touching_right(a, b, v2(0, 0)) ||
                   is_touching_top(a, b, v2(0, 0)) ||
                   is_touching_bottom(a, b, v2(0, 0)));
    return result;
#else
    int d0 = (b.x + b.width)  < a.x;
    int d1 = (a.x + a.width)  < b.x;
    int d2 = (b.y + b.height) < a.y;
    int d3 = (a.y + a.height) < b.y;
    return !(d0 | d1 | d2 | d3);
#endif
}

inline bool are_rect_and_circle_colliding(Rectangle2 rect, Vector2 position, float radius) {
    float closest_x = position.x;
    float closest_y = position.y;
    clamp(&closest_x, rect.x, rect.x + rect.width);
    clamp(&closest_y, rect.y, rect.y + rect.height);

    float dx = position.x - closest_x;
    float dy = position.y - closest_y;

    return (dx * dx + dy * dy) <= (radius * radius);
}
