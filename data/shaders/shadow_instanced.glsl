#version 450
#extension GL_ARB_shading_language_include : require

#define USE_INSTANCING
#include "shader_globals.glsl"

#ifdef VERTEX_SHADER

void main() {
    switch (instance_shadow_cascade_index) {
        case 0: {
            gl_Position = per_scene.light_matrix[0] * instance_world_matrix * vec4(in_position, 1.0);
        } break;

        case 1: {
            gl_Position = per_scene.light_matrix[1] * instance_world_matrix * vec4(in_position, 1.0);
        } break;

        case 2: {
            gl_Position = per_scene.light_matrix[2] * instance_world_matrix * vec4(in_position, 1.0);
        } break;

        case 3: {
            gl_Position = per_scene.light_matrix[3] * instance_world_matrix * vec4(in_position, 1.0);
        } break;
    }
}

#endif

#ifdef FRAGMENT_SHADER

void main() {
    
}

#endif
