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

layout(location = 0) out vec4 output_color;

layout(set = 1, binding = 1) uniform sampler2D hdr_texture;

vec3 aces(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    float exposure = 2.0;

    vec4 hdr_color = texture(hdr_texture, frag_uv);

    vec3 mapped = aces(hdr_color.rgb * exposure);
    output_color = vec4(mapped, 1.0);
}

#endif
