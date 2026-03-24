#version 450
#extension GL_ARB_shading_language_include : require

#define DO_3D_LIGHTING
#include "shader_globals.glsl"

layout(location = 2) COMM vec3 world_normal;
layout(location = 6) COMM vec3 world_position;

#ifdef VERTEX_SHADER

void main() {
    gl_Position = per_scene.projection * per_scene.view * per_object.world * vec4(in_position, 1.0);

    world_position = (per_object.world * vec4(in_position, 1.0)).xyz;
    
    mat3 normal_matrix = transpose(inverse(mat3(per_object.world)));
    world_normal       = (per_object.world * vec4(in_normal, 0.0)).xyz;
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) out vec4 output_normal;
layout(location = 1) out vec4 output_position;

void main() {
    vec3 N        = normalize(world_normal);
    output_normal = vec4(N * 0.5 + 0.5, 1.0);

    output_position = vec4(world_position, 1.0);
}

#endif
