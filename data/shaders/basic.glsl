// file: mesh_shader.glsl

#version 450

layout(location = 0) COMM vec2 frag_uv;
layout(location = 1) COMM vec4 frag_color;

#ifdef VERTEX_SHADER

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_normal;
layout(location = 4) in vec3 in_tangent;
layout(location = 5) in vec3 in_bitangent;

layout(set = 0, binding = 0) uniform PerScene {
    mat4 view;
    mat4 projection;
} per_scene;

layout(set = 1, binding = 0) uniform PerObject {
    mat4 world;
} per_object;

void main() {
    frag_uv = in_uv;
    frag_color = in_color;
    
    gl_Position = per_scene.projection * per_scene.view * per_object.world * vec4(in_position, 1.0);
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) out vec4 output_color;

layout(set = 2, binding = 1) uniform sampler2D albedo_texture;

void main() {
    vec4 tex_color = texture(albedo_texture, frag_uv);
    output_color = tex_color * frag_color;
}

#endif
