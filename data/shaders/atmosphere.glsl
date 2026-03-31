#version 450
#extension GL_ARB_shading_language_include : require

#define USE_IMMEDIATE_VERTEX
#include "shader_globals.glsl"

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

vec3 get_cubemap_direction(int face, vec2 uv) {
    vec2 p = uv * 2.0 - 1.0;
    
    switch (face) {
        case 0: return normalize(vec3( 1.0, -p.y, -p.x)); // +X
        case 1: return normalize(vec3(-1.0, -p.y,  p.x)); // -X
        case 2: return normalize(vec3( p.x,  1.0,  p.y)); // +Y
        case 3: return normalize(vec3( p.x, -1.0, -p.y)); // -Y
        case 4: return normalize(vec3( p.x, -p.y,  1.0)); // +Z
        case 5: return normalize(vec3(-p.x, -p.y, -1.0)); // -Z
    }

    return vec3(0.0);
}

#define ATMOSPHERE_CUBEMAP_SIZE 128

void main() {
    vec2 uv  = (gl_FragCoord.xy + 0.5) / vec2(ATMOSPHERE_CUBEMAP_SIZE);
    vec3 dir = get_cubemap_direction(per_scene.cubemap_face, frag_uv);

    float t = dir.y * 0.5 + 0.5;

    vec3 horizon_color = vec3(0.6, 0.7, 0.9);
    vec3 zenith_color  = vec3(0.1, 0.3, 0.8);

    vec3 color = mix(horizon_color, zenith_color, t);

    vec3 sun_dir = vec3(0.0);
    for (int i = 0; i < MAX_LIGHTS; i++) {
        Light light = per_scene.lights[i];

        if (light.type == LIGHT_TYPE_DIRECTIONAL) {
            sun_dir = light.direction;
            sun_dir.y = -sun_dir.y;
            break;
        }
    }
    
    float sun_amount = max(dot(dir, normalize(sun_dir)), 0.0);
    float sun        = pow(sun_amount, 512.0);

    color += vec3(1.0, 0.9, 0.6) * sun * 5.0;
    
    output_color = vec4(color, 1.0);
}

#endif
