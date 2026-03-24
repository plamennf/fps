#version 450
#extension GL_ARB_shading_language_include : require

#define USE_INSTANCING
#define DO_3D_LIGHTING
#include "shader_globals.glsl"

layout(location = 0) COMM vec2 frag_uv;

#ifdef VERTEX_SHADER

void main() {
    gl_Position = per_scene.projection * per_scene.view * instance_world_matrix * vec4(in_position, 1.0);
    frag_uv = vec2(in_uv.x, 1.0 - in_uv.y);
}

#endif

#ifdef FRAGMENT_SHADER

void main() {
    vec4 full_albedo = texture(albedo_texture, frag_uv);
    if (full_albedo.a < 0.5) discard;
    //discard;
}

#endif
