#version 450
#extension GL_ARB_shading_language_include : require

#include "shader_globals.glsl"

layout(location = 0) COMM vec3 frag_uv;
layout(location = 1) COMM vec3 world_position;

#ifdef VERTEX_SHADER

void main() {
    gl_Position = per_scene.projection * mat4(mat3(per_scene.view)) * vec4(in_position, 1.0);
    frag_uv     = in_position;
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) out vec4 output_color;

layout(set = 0, binding = 5) uniform samplerCube skybox_texture;

void main() {
    vec3 color = texture(skybox_texture, normalize(frag_uv)).rgb;

    output_color = vec4(color, 1.0);
}

#endif
