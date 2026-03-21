#version 450

layout(location = 0) COMM vec2 frag_uv;
layout(location = 1) COMM vec4 frag_color;

#ifdef VERTEX_SHADER

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;

layout(set = 0, binding = 0) uniform Per_Scene {
    mat4 projection;
    mat4 view;
} per_scene;

void main() {
    gl_Position = per_scene.projection * per_scene.view * vec4(in_position, 0.0, 1.0);
    frag_color  = in_color;
    frag_uv     = in_uv;
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) out vec4 output_color;

layout(set = 1, binding = 0) uniform sampler2D hdr_texture;

void main() {
    float exposure = 2.0;

    vec4 hdr_color = texture(hdr_texture, frag_uv);

    vec3 mapped = 1.0 - exp(-hdr_color.rgb * exposure);
    output_color = vec4(mapped, 1.0);
}

#endif
