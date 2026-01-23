const int NUM_SHADOW_MAP_CASCADES = 4;

OUT_IN vec2 vertex_uv;
OUT_IN vec3 world_normal;
OUT_IN vec3 world_position;
OUT_IN vec3 vertex_pos_view_space;
OUT_IN mat3 TBN;
OUT_IN float clip_space_pos_z;

#ifdef VERTEX_SHADER

layout(location = 0) in vec3 input_position;
layout(location = 1) in vec2 input_uv;
layout(location = 2) in vec3 input_normal;
layout(location = 3) in vec3 input_tangent;
layout(location = 4) in vec3 input_bitangent;

uniform mat4 projection_matrix;
uniform mat4 view_matrix;
uniform mat4 world_matrix;

void main(void) {
    gl_Position = projection_matrix * view_matrix * world_matrix * vec4(input_position, 1.0);
    vertex_uv = vec2(input_uv.x, 1.0 - input_uv.y);

    world_normal   = mat3(transpose(inverse(world_matrix))) * input_normal;
    world_position = (world_matrix * vec4(input_position.xyz, 1.0)).xyz;
    vertex_pos_view_space  = (view_matrix * vec4(world_position, 1.0)).xyz;
    //vertex_pos_light_space = light_matrix * vec4(world_position, 1.0);
    clip_space_pos_z = gl_Position.z;
    
#if 0
    vec3 T = normalize(vec3(world_matrix * vec4(input_tangent,   0.0)));
    vec3 B = normalize(vec3(world_matrix * vec4(input_bitangent, 0.0)));
    vec3 N = normalize(vec3(world_matrix * vec4(input_normal,    0.0)));
#else
    vec3 T = normalize(vec3(world_matrix * vec4(input_tangent,   0.0)));
    vec3 N = normalize(vec3(world_matrix * vec4(input_normal,    0.0)));

    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
#endif
    
    TBN = transpose(mat3(T, B, N));
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) out vec4 output_color;

struct Material {
    vec4 color;
    float shininess;
    int use_normal_map;
};

uniform Material material;
uniform vec3 camera_position;

uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D normal_texture;
uniform sampler2D metallic_roughness_texture;
uniform sampler2D shadow_map_textures[NUM_SHADOW_MAP_CASCADES];

uniform mat4 light_matrices[NUM_SHADOW_MAP_CASCADES];
uniform float cascade_splits[NUM_SHADOW_MAP_CASCADES];

struct Directional_Light {
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct Point_Light {
    vec3 position;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct Spot_Light {
    vec3 position;
    vec3 direction;
    float cut_off;
    float outer_cut_off;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform Directional_Light directional_light;

#define MAX_POINT_LIGHTS 4
uniform Point_Light point_lights[MAX_POINT_LIGHTS];

uniform Spot_Light spot_light;

vec3 calculate_directional_light(Directional_Light light, vec3 normal, vec3 view_dir, vec4 diffuse_color, vec4 specular_color, float shadow) {
    if (length(light.diffuse) + length(light.specular) + length(light.ambient) < 1e-6) {
        return vec3(0.0);
    }
    
    vec3 light_dir = normalize(-light.direction);
    if (material.use_normal_map == 1) light_dir *= TBN;
    
    //vec3 light_dir = normalize(light.direction);
    float diff = max(dot(normal, light_dir), 0.0);
    vec3 reflect_dir = reflect(-light_dir, normal);

    vec3 halfway_dir = normalize(light_dir + view_dir);
        
    //float spec = pow(max(dot(view_dir, reflect_dir), 0.0), material.shininess);
    float spec = pow(max(dot(normal, halfway_dir), 0.0), material.shininess);
    
    vec3 ambient  = light.ambient  * diffuse_color.rgb;
    vec3 diffuse  = light.diffuse  * diff * diffuse_color.rgb;
    vec3 specular = light.specular * spec * specular_color.rgb;
    return (ambient + ((1.0 - shadow) * (diffuse + specular)));
    //return (ambient + diffuse + specular);
}

vec3 calculate_point_light(Point_Light light, vec3 normal, vec3 frag_pos, vec3 view_dir, vec4 diffuse_color, vec4 specular_color) {
    if (length(light.diffuse) + length(light.specular) + length(light.ambient) < 1e-6) {
        return vec3(0.0);
    }
    
    vec3 light_dir = normalize(light.position - frag_pos);
    if (material.use_normal_map == 1) light_dir *= TBN;
    
    float diff = max(dot(normal, light_dir), 0.0);
    vec3 reflect_dir = reflect(-light_dir, normal);
    
    vec3 halfway_dir = normalize(light_dir + view_dir);
        
    //float spec = pow(max(dot(view_dir, reflect_dir), 0.0), material.shininess);
    float spec = pow(max(dot(normal, halfway_dir), 0.0), material.shininess);
    
    float dist = length(light.position - frag_pos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * (dist * dist));
    vec3 ambient  = light.ambient  * diffuse_color.rgb;
    vec3 diffuse  = light.diffuse  * diff * diffuse_color.rgb;
    vec3 specular = light.specular * spec * specular_color.rgb;
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

vec3 calculate_spot_light(Spot_Light light, vec3 normal, vec3 frag_pos, vec3 view_dir, vec4 diffuse_color, vec4 specular_color) {
    if (length(light.diffuse) + length(light.specular) + length(light.ambient) < 1e-6) {
        return vec3(0.0);
    }

    vec3 light_dir = normalize(light.position - frag_pos);
    float theta = dot(light_dir, normalize(-light.direction));
    float epsilon = light.cut_off - light.outer_cut_off;
    float intensity = clamp((theta - light.outer_cut_off) / epsilon, 0.0, 1.0);
    
    if (theta > light.cut_off) {
        vec3 light_dir = normalize(light.position - frag_pos);
        if (material.use_normal_map == 1) light_dir *= TBN;
        
        float diff = max(dot(normal, light_dir), 0.0);
        vec3 reflect_dir = reflect(-light_dir, normal);

        vec3 halfway_dir = normalize(light_dir + view_dir);
        
        //float spec = pow(max(dot(view_dir, reflect_dir), 0.0), material.shininess);
        float spec = pow(max(dot(normal, halfway_dir), 0.0), material.shininess);
        float dist = length(light.position - frag_pos);
        float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * (dist * dist));
        vec3 ambient  = light.ambient  * diffuse_color.rgb;
        vec3 diffuse  = light.diffuse  * diff * diffuse_color.rgb;
        vec3 specular = light.specular * spec * specular_color.rgb;
        ambient  *= attenuation;
        diffuse  *= attenuation * intensity;
        specular *= attenuation * intensity;
        return (ambient + diffuse + specular);
    } else {
        return light.ambient * diffuse_color.rgb;
    }
}

int calculate_csm() {
    float depth = length(world_position - camera_position);
    
    int index = NUM_SHADOW_MAP_CASCADES - 1;
    if (depth < cascade_splits[0]) index = 0;
    else if (depth < cascade_splits[1]) index = 1;
    else if (depth < cascade_splits[2]) index = 2;

    return index;
}

float calculate_shadow_per_cascade(vec4 vertex_pos_light_space, int index) {
    vec3 proj_coords = vertex_pos_light_space.xyz / vertex_pos_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5;

    if (proj_coords.z > 1.0) return 0.0;
    if (proj_coords.x < 0.0 || proj_coords.x > 1.0 || proj_coords.y < 0.0 || proj_coords.y > 1.0) return 0.0;
    
    float closest_depth = texture(shadow_map_textures[index], proj_coords.xy).r;
    float current_depth = proj_coords.z;
    //float bias = max(0.01 * (1.0 - dot(normalize(world_normal), directional_light.direction)), 0.001);
    float bias = 0.0001;

    float shadow = 0.0;
    vec2 texel_size = 1.0 / textureSize(shadow_map_textures[index], 0);
    if (proj_coords.z < 1.0) {
        for (int x = -2; x <= 2; ++x) {
            for (int y = -2; y <= 2; ++y) {
                float pcf_depth = texture(shadow_map_textures[index], proj_coords.xy + vec2(x, y) * texel_size).r;
                shadow += current_depth - bias > pcf_depth ? 1.0 : 0.0;
            }
        }
    }
    shadow /= 25.0;
    
    return shadow;
}

float calculate_shadow() {
    int index = calculate_csm();
    vec4 vertex_pos_light_space = light_matrices[index] * vec4(world_position, 1.0);

    if (index == 0) return calculate_shadow_per_cascade(vertex_pos_light_space, 0);
    if (index == 1) return calculate_shadow_per_cascade(vertex_pos_light_space, 1);
    if (index == 2) return calculate_shadow_per_cascade(vertex_pos_light_space, 2);
    return calculate_shadow_per_cascade(vertex_pos_light_space, 3);
}

void main(void) {
    vec4 diffuse_color  = texture(diffuse_texture, vertex_uv) * material.color;
    vec4 specular_color = texture(specular_texture, vertex_uv);
    
    vec3 view_dir = normalize(camera_position - world_position);
    vec3 normal = normalize(world_normal);

    if (material.use_normal_map == 1) {
        normal = texture(normal_texture, vertex_uv).xyz;
        normal = normal * 2.0 - 1.0;
        normal = normalize(TBN * normal);

        view_dir *= TBN;
    }

    float shadow = calculate_shadow();
    vec3 accum = calculate_directional_light(directional_light, normal, view_dir, diffuse_color, specular_color, shadow);

    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        accum += calculate_point_light(point_lights[i], normal, world_position, view_dir, diffuse_color, specular_color);
    }

    accum += calculate_spot_light(spot_light, normal, world_position, view_dir, diffuse_color, specular_color);
    
    output_color = vec4(accum, 1.0);
    //output_color = diffuse_color;
}

#endif
