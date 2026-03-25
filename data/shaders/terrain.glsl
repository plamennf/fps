#version 450
#extension GL_ARB_shading_language_include : require

#define DO_3D_LIGHTING
#define DO_TERRAIN_AO
#include "shader_globals.glsl"

layout(location = 0) COMM vec2 frag_uv;
layout(location = 1) COMM vec4 frag_color;
layout(location = 2) COMM vec3 world_normal;
layout(location = 3) COMM mat3 TBN;
layout(location = 6) COMM vec3 world_position;
layout(location = 7) COMM vec3 view_position;

#ifdef VERTEX_SHADER

void main() {
    frag_uv = vec2(in_uv.x, 1.0 - in_uv.y);
    frag_color = in_color * per_object.scale_color;
    
    gl_Position = per_scene.projection * per_scene.view * per_object.world * vec4(in_position, 1.0);

    world_position = (per_object.world * vec4(in_position, 1.0)).xyz;
    view_position  = (per_scene.view * vec4(world_position, 1.0)).xyz;
    
    mat3 normal_matrix = transpose(inverse(mat3(per_object.world)));
    world_normal       = normal_matrix * in_normal;

    vec3 T = normalize((per_object.world * vec4(in_tangent, 0.0)).xyz);
    vec3 N = normalize(world_normal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) out vec4 output_color;

void main() {
    vec3 color   = calculate_lighting(frag_uv, frag_color, world_normal, TBN, world_position, view_position);
    output_color = vec4(color, 1.0);
}

#endif
