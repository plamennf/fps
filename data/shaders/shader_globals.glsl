#define MAX_SHADOW_CASCADES 4
//#define DISABLE_SHADOW_BLEND

#define SHADOW_MAP_WIDTH  4096
#define SHADOW_MAP_HEIGHT SHADOW_MAP_WIDTH

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
    mat4 light_matrix[MAX_SHADOW_CASCADES];
    vec4 cascade_splits[MAX_SHADOW_CASCADES];
    Light lights[MAX_LIGHTS];
    vec3 camera_position;
    int cubemap_face;
    vec2 extent;
} per_scene;

layout(set = 1, binding = 0) uniform Material {
    vec4 albedo_factor;
    vec3 emissive_factor;
    int uses_specular_glossiness;
    int has_normal_map;
} material;

#ifdef VERTEX_SHADER

#ifndef USE_IMMEDIATE_VERTEX
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_normal;
layout(location = 4) in vec3 in_tangent;
layout(location = 5) in vec3 in_bitangent;
#endif

#ifdef USE_INSTANCING

layout(location = 6) in mat4 instance_world_matrix;
layout(location = 10) in vec4 instance_scale_color;
layout(location = 11) in int instance_shadow_cascade_index;

#else

layout(push_constant) uniform Per_Object {
    mat4 world;
    vec4 scale_color;
    int shadow_cascade_index;
} per_object;

#endif

#endif

#ifdef DO_3D_LIGHTING

layout(set = 1, binding = 1) uniform sampler2D albedo_texture;
layout(set = 1, binding = 2) uniform sampler2D normal_texture;
layout(set = 1, binding = 3) uniform sampler2D metallic_roughness_texture;
layout(set = 1, binding = 4) uniform sampler2D ao_texture;
layout(set = 1, binding = 5) uniform sampler2D emissive_texture;
layout(set = 1, binding = 6) uniform sampler2D terrain_ao;

layout(set = 0, binding = 1) uniform sampler2D shadow_map_0;
layout(set = 0, binding = 2) uniform sampler2D shadow_map_1;
layout(set = 0, binding = 3) uniform sampler2D shadow_map_2;
layout(set = 0, binding = 4) uniform sampler2D shadow_map_3;
layout(set = 0, binding = 5) uniform samplerCube skybox_texture;

struct Cascade_Data {
    int index0;
    int index1;
    float blend;
};

Cascade_Data get_cascade_blend(float depth) {
    Cascade_Data data;
    const float delta = 10.0;
    data.index0 = 0;
    data.index1 = 0;
    data.blend  = 0.0;

    for (int i = 0; i < MAX_SHADOW_CASCADES-1; i++) {
        float split_near = per_scene.cascade_splits[i].x - delta;
        float split_far  = per_scene.cascade_splits[i].x + delta;

        if (depth < split_near) {
            data.index0 = i;
            data.index1 = i;
            data.blend  = 0.0;
            return data;
        } else if (depth < split_far) {
            data.index0 = i;
            data.index1 = i+1;
            data.blend  = smoothstep(split_near, split_far, depth);
            return data;
        }
    }

    data.index0 = MAX_SHADOW_CASCADES-1;
    data.index1 = MAX_SHADOW_CASCADES-1;
    data.blend  = 0.0;
    return data;
}

int calculate_cascade_index(vec3 world_position, vec3 camera_position) {
    //float depth = length(world_position - camera_position);
    vec3 view_pos = (per_scene.view * vec4(world_position, 1.0)).xyz;
    float depth   = -view_pos.z; // because -z is forward

    int index = MAX_SHADOW_CASCADES - 1;
    if (depth < per_scene.cascade_splits[0].x) index = 0;
    else if (depth < per_scene.cascade_splits[1].x) index = 1;
    else if (depth < per_scene.cascade_splits[2].x) index = 2;

    return index;
}

float pcf_shadow(vec3 proj_coords, int cascade_index, float bias) {
    float shadow = 0.0;
    float total_weight = 0.0;
    
    float current_depth = proj_coords.z;

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float s = 0.0;
            switch (cascade_index) {
                case 0: {
                    vec2 offset = vec2(x, y) / vec2(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);
                    float shadow_depth = texture(shadow_map_0, proj_coords.xy + offset).r;
                    s = current_depth - bias > shadow_depth ? 0.0 : 1.0;
                } break;

                case 1: {
                    vec2 offset = vec2(x, y) / vec2(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);
                    float shadow_depth = texture(shadow_map_1, proj_coords.xy + offset).r;
                    s = current_depth - bias > shadow_depth ? 0.0 : 1.0;
                } break;

                case 2: {
                    vec2 offset = vec2(x, y) / vec2(SHADOW_MAP_WIDTH / 2, SHADOW_MAP_HEIGHT / 2);
                    float shadow_depth = texture(shadow_map_2, proj_coords.xy + offset).r;
                    s = current_depth - bias > shadow_depth ? 0.0 : 1.0;
                } break;

                case 3: {
                    vec2 offset = vec2(x, y) / vec2(SHADOW_MAP_WIDTH / 4, SHADOW_MAP_HEIGHT / 4);
                    float shadow_depth = texture(shadow_map_3, proj_coords.xy + offset).r;
                    s = current_depth - bias > shadow_depth ? 0.0 : 1.0;
                } break;
            }

            float weight = 1.0 / (1.0 + length(vec2(x, y)));
            total_weight += weight;
            shadow += s * weight;
        }
    }

    shadow /= total_weight;
    return shadow;
}

float calculate_shadow(int cascade_index, vec3 world_position, float bias) {
    vec4 shadow_pos = per_scene.light_matrix[cascade_index] * vec4(world_position, 1.0);
    
    vec3 proj_coords = shadow_pos.xyz / shadow_pos.w;
    proj_coords.x = proj_coords.x * 0.5 + 0.5;
    //proj_coords.y = proj_coords.y * -0.5 + 0.5; // This is for directx 11
    proj_coords.y = proj_coords.y * 0.5 + 0.5; // This is for vulkan

    float shadow = pcf_shadow(proj_coords, cascade_index, bias);
    return shadow;
}

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

vec3 calculate_lighting(vec2 frag_uv, vec4 frag_color, vec3 world_normal, mat3 TBN, vec3 world_position, vec3 view_position) {
    vec4 full_albedo = texture(albedo_texture, frag_uv);
#ifdef FRAGMENT_SHADER
    if (full_albedo.a < 0.5) discard;
#endif
    
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

#ifdef DISABLE_SHADOW_BLEND
    int cascade_index = calculate_cascade_index(world_position, per_scene.camera_position);
#endif
    
    vec3 Lo = vec3(0, 0, 0);
    for (int i = 0; i < MAX_LIGHTS; i++) {
        Light light = per_scene.lights[i];
        
        vec3 L;
        float attenuation = 1.0;

        float s = 1.0;
        switch (light.type) {
            case LIGHT_TYPE_DIRECTIONAL: {
                L = normalize(-light.direction);
                //float bias = 0.01;
                float bias = max(0.0005 * (1.0 - dot(N, L)), 0.01);
#ifdef DISABLE_SHADOW_BLEND
                s = calculate_shadow(cascade_index, world_position, bias);
#else
                vec3 view_pos = (per_scene.view * vec4(world_position, 1.0)).xyz;
                Cascade_Data cascade = get_cascade_blend(-view_pos.z);
                float s0 = calculate_shadow(cascade.index0, world_position, bias);
                float s1 = calculate_shadow(cascade.index1, world_position, bias);
                s  = mix(s0, s1, cascade.blend);
#endif
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

#ifdef DO_TERRAIN_AO
    ao = texture(terrain_ao, frag_uv).r;
    vec3 ambient = albedo * ao * 0.03;
#else
    vec3 ambient    = albedo * ao * 0.03;
    vec3 irradiance = texture(skybox_texture, N).rgb;
    ambient += albedo * irradiance * 0.3;
    
    {
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 R  = reflect(-V, N);
        vec3 prefiltered_color = texture(skybox_texture, R).rgb;
        vec3 F  = fresnel_schlick(max(dot(N, V), 0.0), F0);
        vec3 specular = F * prefiltered_color;

        ambient += specular * 0.5;
    }
#endif
    vec3 color   = ambient + Lo + emissive;

    //const float density  = 0.007;
    const float density  = 0.005;
    const float gradient = 1.5;
    
    float distance_to_camera = length(view_position);
    float visibility = exp(-pow((distance_to_camera*density), gradient));
    visibility = clamp(visibility, 0.0, 1.0);
    vec3 sky_color = vec3(0.2, 0.5, 0.8);
    color = mix(sky_color, color, visibility);
    
    return color;
}

#endif
