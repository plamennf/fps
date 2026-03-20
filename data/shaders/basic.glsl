// file: mesh_shader.glsl

#version 450

layout(location = 0) COMM vec2 frag_uv;
layout(location = 1) COMM vec4 frag_color;
layout(location = 2) COMM vec3 world_normal;
layout(location = 3) COMM mat3 TBN;
layout(location = 6) COMM vec3 world_position;

#define PI 3.14159265359

#define MAX_LIGHTS 8

#define LIGHT_TYPE_UNKNOWN     0
#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_POINT       2
#define LIGHT_TYPE_SPOT        3

struct Light {
    int type;
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float range;
    float spot_inner_cone_angle;
    float spot_outer_cone_angle;
};

layout(set = 0, binding = 0) uniform Per_Scene {
    mat4 projection;
    mat4 view;
    Light lights[MAX_LIGHTS];
    vec3 camera_position;
    float _padding;
} per_scene;

layout(set = 1, binding = 0) uniform Material {
    vec4 albedo_factor;
    vec3 emissive_factor;
    int uses_specular_glossiness;
    int has_normal_map;
} material;

#ifdef VERTEX_SHADER

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_normal;
layout(location = 4) in vec3 in_tangent;
layout(location = 5) in vec3 in_bitangent;

layout(push_constant) uniform Per_Object {
    mat4 world;
    vec4 scale_color;
} per_object;

void main() {
    frag_uv = vec2(in_uv.x, 1.0 - in_uv.y);
    frag_color = in_color * per_object.scale_color;
    
    gl_Position = per_scene.projection * per_scene.view * per_object.world * vec4(in_position, 1.0);

    world_position = (per_object.world * vec4(in_position, 1.0)).xyz;
    world_normal   = (per_object.world * vec4(in_normal,   0.0)).xyz;

    vec3 T = normalize((per_object.world * vec4(in_tangent, 0.0)).xyz);
    vec3 N = normalize(world_normal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    TBN = transpose(mat3(T, B, N));
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) out vec4 output_color;

layout(set = 1, binding = 1) uniform sampler2D albedo_texture;
layout(set = 1, binding = 2) uniform sampler2D normal_texture;
layout(set = 1, binding = 3) uniform sampler2D metallic_roughness_texture;
layout(set = 1, binding = 4) uniform sampler2D ao_texture;
layout(set = 1, binding = 5) uniform sampler2D emissive_texture;

float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num    = a2;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom        = PI * denom * denom;

    return num / denom;
}

float geometry_schlick_ggx(float NdotV, float roughness) {
    float r     = (roughness + 1.0);
    float k     = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = geometry_schlick_ggx(NdotV, roughness);
    float ggx1  = geometry_schlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

void main() {
    vec4 full_albedo = texture(albedo_texture, frag_uv);
    vec3 albedo      = full_albedo.rgb * material.albedo_factor.xyz * frag_color.rgb;

    vec3 normal      = world_normal;

    vec4 full_mr = texture(metallic_roughness_texture, frag_uv);
    float metallic;
    float roughness;
    vec3 F0;
    if (material.uses_specular_glossiness == 1) {
        roughness = 1.0 - full_mr.a;
        metallic  = 0.0;

        F0        = full_mr.rgb;
    } else {
        roughness = full_mr.g;
        metallic  = full_mr.b;

        F0 = vec3(0.04, 0.04, 0.04);
        F0 = mix(F0, albedo, metallic);
    }

    float ao       = texture(ao_texture, frag_uv).r;
    vec3  emissive = texture(emissive_texture, frag_uv).rgb * material.emissive_factor;

    vec3 N = normalize(normal);
    vec3 V = normalize(per_scene.camera_position - world_position);

    if (material.has_normal_map == 1) {
        normal = texture(normal_texture, frag_uv).xyz;
        normal = normal * 2.0 - 1.0;
        normal = normalize(normal);

        N = normalize(TBN * normal);
    }

    vec3 Lo = vec3(0, 0, 0);
    for (int i = 0; i < MAX_LIGHTS; i++) {
        Light light = per_scene.lights[i];
        
        vec3 L;
        float attenuation = 1.0;

        float s = 1.0;
        switch (light.type) {
            case LIGHT_TYPE_DIRECTIONAL: {
                L = normalize(-light.direction);
                // s = shadow;
            } break;

            case LIGHT_TYPE_POINT: {
                L = normalize(light.position - world_position);
                float dist = length(light.position - world_position);
                attenuation = 1.0 / (dist * dist);
            } break;

            case LIGHT_TYPE_SPOT: {
                L = normalize(light.position - world_position);
                float dist = length(light.position - world_position);
                attenuation = 1.0 / (dist * dist);

                vec3  spot_dir  = normalize(-light.direction);
                float cos_theta = dot(L, spot_dir);
                float epsilon   = light.spot_inner_cone_angle - light.spot_outer_cone_angle;
                float intensity = clamp((cos_theta - light.spot_outer_cone_angle) / epsilon, 0.0, 1.0);
                attenuation *= intensity;
            } break;

            default: {
                L = vec3(0, 0, 0);
            } break;
        }

        vec3 H        = normalize(V + L);
        vec3 radiance = light.color * attenuation * light.intensity;

        float NDF = distribution_ggx(N, H, roughness);
        float G   = geometry_smith(N, V, L, roughness);
        vec3  F   = fresnel_schlick(max(dot(H, V), 0.0), F0);

        vec3  kS  = F;
        vec3  kD  = vec3(1) - kS;
        kD *= 1.0 - metallic;

        vec3  numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular    = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0);
        Lo += ((kD * albedo / PI + specular) * radiance * NdotL) * s;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color   = ambient + Lo + emissive;

    output_color = vec4(color, 1.0);
}

#endif
