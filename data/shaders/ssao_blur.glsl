#version 450
#extension GL_ARB_shading_language_include : require

layout(location = 0) COMM vec2 frag_uv;
layout(location = 1) COMM vec4 frag_color;

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

layout(set = 1, binding = 1) uniform sampler2D ao_texture;
layout(set = 1, binding = 2) uniform sampler2D normal_texture;
layout(set = 1, binding = 3) uniform sampler2D depth_texture;

void main() {
    vec2 texel_size = 1.0 / vec2(textureSize(ao_texture, 0));

    float result = 0.0;

    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texel_size;
            result += texture(ao_texture, frag_uv + offset).r;
        }
    }

    output_ao = result / 16.0;
}

#endif
