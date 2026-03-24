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

const int radius = 4;
const float weights[2*radius+1] = float[](
    0.05, 0.09, 0.12, 0.15, 0.18, 0.15, 0.12, 0.09, 0.05
);

void main()
{
    vec2 texel_size = 1.0 / vec2(textureSize(ao_texture, 0));

    float sum = 0.0;
    for (int i = -radius; i <= radius; i++) {
        vec2 offset = vec2(float(i), 0.0) * texel_size;
        sum += texture(ao_texture, frag_uv + offset).r * weights[i + radius];
    }

    float horizontal = sum;

    sum = 0.0;
    for (int i = -radius; i <= radius; i++) {
        vec2 offset = vec2(0.0, float(i)) * texel_size;
        sum += texture(ao_texture, frag_uv + offset).r * weights[i + radius];
    }

    float vertical = sum;

    output_ao = (horizontal + vertical) * 0.5;
}

#endif
