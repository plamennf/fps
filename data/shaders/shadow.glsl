#version 450

#define MAX_SHADOW_CASCADES 4

#define PI 3.14159265359

#define MAX_LIGHTS 8

#define LIGHT_TYPE_UNKNOWN     0
#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_POINT       2
#define LIGHT_TYPE_SPOT        3

struct Light {
    int type;
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float range;
    float spot_inner_cone_angle;
    float spot_outer_cone_angle;
};

layout(set = 0, binding = 0) uniform Per_Scene {
    mat4 projection;
    mat4 view;
    mat4 light_matrix[MAX_SHADOW_CASCADES];
    vec4 cascade_splits[MAX_SHADOW_CASCADES];
    Light lights[MAX_LIGHTS];
    vec3 camera_position;
    int shadow_cascade_index;
} per_scene;

layout(set = 1, binding = 0) uniform Material {
    vec4 albedo_factor;
    vec3 emissive_factor;
    int uses_specular_glossiness;
    int has_normal_map;
} material;

#ifdef VERTEX_SHADER

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_normal;
layout(location = 4) in vec3 in_tangent;
layout(location = 5) in vec3 in_bitangent;

layout(push_constant) uniform Per_Object {
    mat4 world;
    vec4 scale_color;
    int shadow_cascade_index;
} per_object;

void main() {
    switch (per_object.shadow_cascade_index) {
        case 0: {
            gl_Position = per_scene.light_matrix[0] * per_object.world * vec4(in_position, 1.0);
        } break;

        case 1: {
            gl_Position = per_scene.light_matrix[1] * per_object.world * vec4(in_position, 1.0);
        } break;

        case 2: {
            gl_Position = per_scene.light_matrix[2] * per_object.world * vec4(in_position, 1.0);
        } break;

        case 3: {
            gl_Position = per_scene.light_matrix[3] * per_object.world * vec4(in_position, 1.0);
        } break;
    }
}

#endif

#ifdef FRAGMENT_SHADER

void main() {
    
}

#endif
