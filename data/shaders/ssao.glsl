#version 450
#extension GL_ARB_shading_language_include : require

#define USE_IMMEDIATE_VERTEX
#include "shader_globals.glsl"

layout(location = 0) COMM vec2 frag_uv;
layout(location = 1) COMM vec4 frag_color;

#define SSAO_KERNEL_SIZE 192

layout(set = 1, binding = 0) uniform SSAO_Kernel {
    vec4 ssao_kernel[SSAO_KERNEL_SIZE];
};

#ifdef VERTEX_SHADER

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;

void main() {
    gl_Position = vec4(in_position, 0.0, 1.0);
    frag_color  = in_color;
    frag_uv     = in_uv;
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) out float output_ao;

layout(set = 1, binding = 1) uniform sampler2D position_texture;
layout(set = 1, binding = 2) uniform sampler2D normal_texture;
layout(set = 1, binding = 3) uniform sampler2D depth_texture;
layout(set = 1, binding = 4) uniform sampler2D noise_texture;

void main() {
    vec3 frag_pos   = texture(position_texture, frag_uv).xyz;
    vec3 normal     = normalize(texture(normal_texture, frag_uv).xyz * 2.0 - 1.0);
    vec3 random_vec = normalize(texture(noise_texture,  frag_uv * per_scene.ssao_noise_scale).xyz) * 0.7;
    vec3 tangent    = normalize(random_vec - normal * dot(random_vec, normal));
    vec3 bitangent  = cross(normal, tangent);
    mat3 TBN        = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < SSAO_KERNEL_SIZE; i++) {
        vec3 sample_pos = TBN * ssao_kernel[i].xyz;
        sample_pos      = frag_pos + sample_pos * per_scene.ssao_radius;

        vec4 offset     = vec4(sample_pos, 1.0);
        offset          = per_scene.projection * offset;
        offset.xyz     /= offset.w;
        offset.xyz      = offset.xyz * 0.5 + 0.5;

        float sample_depth = texture(position_texture, offset.xy).z;

#if 0
        float range_check  = smoothstep(0.0, 1.0, per_scene.ssao_radius / abs(frag_pos.z - sample_depth));
        occlusion         += (sample_depth >= sample_pos.z + per_scene.ssao_bias ? 1.0 : 0.0) * range_check;
#else
        float range_check  = smoothstep(0.0, 1.0, per_scene.ssao_radius / abs(frag_pos.z - sample_depth));
        float diff = sample_depth - sample_pos.z;
        float contrib = smoothstep(0.0, per_scene.ssao_radius, max(diff, 0.0));
        occlusion += contrib * range_check;
#endif
    }

    occlusion = 1.0 - (occlusion / SSAO_KERNEL_SIZE);
    output_ao = occlusion;
}

#endif
