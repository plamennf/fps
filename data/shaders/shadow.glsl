#version 450
#extension GL_ARB_shading_language_include : require

#include "shader_globals.glsl"

#ifdef VERTEX_SHADER

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
