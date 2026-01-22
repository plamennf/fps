OUT_IN vec4 vertex_color;
OUT_IN vec2 vertex_uv;
OUT_IN vec2 texel_step;

#ifdef VERTEX_SHADER

layout(location = 0) in vec2 input_position;
layout(location = 1) in vec4 input_color;
layout(location = 2) in vec2 input_uv;

uniform mat4 projection_matrix;
uniform mat4 view_matrix;
uniform mat4 world_matrix;

void main(void) {
    gl_Position  = projection_matrix * view_matrix * world_matrix * vec4(input_position, 0.0, 1.0);
    vertex_color = input_color;
    vertex_uv    = input_uv;

    texel_step.x = 2.0f / projection_matrix[0][0];
    texel_step.y = 2.0f / projection_matrix[1][1];
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) out vec4 output_color;

uniform sampler2D diffuse_texture;

void main(void) {
    vec3 rgb_m = texture(diffuse_texture, vertex_uv).rgb;

    vec3 rgb_nw = textureOffset(diffuse_texture, vertex_uv, ivec2(-1, +1)).rgb;
    vec3 rgb_ne = textureOffset(diffuse_texture, vertex_uv, ivec2(+1, +1)).rgb;
    vec3 rgb_sw = textureOffset(diffuse_texture, vertex_uv, ivec2(-1, -1)).rgb;
    vec3 rgb_se = textureOffset(diffuse_texture, vertex_uv, ivec2(+1, -1)).rgb;

    // http://en.wikipedia.org/wiki/Grayscale
    const vec3 to_luma = vec3(0.299, 0.587, 0.114);

    float luma_nw = dot(rgb_nw, to_luma);
    float luma_ne = dot(rgb_ne, to_luma);
    float luma_sw = dot(rgb_sw, to_luma);
    float luma_se = dot(rgb_se, to_luma);
    float luma_m  = dot(rgb_m, to_luma);

    float luma_min = min(luma_m, min(min(luma_nw, luma_ne), min(luma_sw, luma_se)));
    float luma_max = max(luma_m, max(max(luma_nw, luma_ne), max(luma_sw, luma_se)));

    const float LUMA_THRESHOLD = 0.5f;
    const float MUL_REDUCE = 1.0f / 80.0f;
    const float MIN_REDUCE = 1.0f / 128.0f;
    const float MAX_SPAN = 8.0f;
    
    if (luma_max - luma_min <= luma_max * LUMA_THRESHOLD) {
        output_color = vec4(rgb_m, 1.0);
        return;
    }

    // Sampling is done along the gradient.
    vec2 sampling_direction;
    sampling_direction.x = -((luma_nw + luma_ne) - (luma_sw + luma_se));
    sampling_direction.y =  ((luma_nw + luma_sw) - (luma_ne + luma_se));

    // Sampling step distance depends on the luma: The brighter the sampled texels, the smaller the final sampling step direction.
    // This results, that brighter areas are less blurred/more sharper than dark areas.
    float sampling_direction_reduce = max((luma_nw + luma_ne + luma_sw + luma_se) * 0.25 * MUL_REDUCE, MIN_REDUCE);

    // Factor for norming the sampling direction plus adding the brightness influence.
    float min_sampling_direction_factor = 1.0 / (min(abs(sampling_direction.x), abs(sampling_direction.y)) + sampling_direction_reduce);

    // Calculate final sampling direction vector by reducing, clamping to a range and finally adapting to the texture size.
    sampling_direction = clamp(sampling_direction * min_sampling_direction_factor, vec2(-MAX_SPAN), vec2(MAX_SPAN)) * texel_step;

    // Inner samples on the tab.
    vec3 rgb_sample_neg = texture(diffuse_texture, vertex_uv + sampling_direction * (1.0/3.0 - 0.5)).rgb;
    vec3 rgb_sample_pos = texture(diffuse_texture, vertex_uv + sampling_direction * (2.0/3.0 - 0.5)).rgb;

    vec3 rgb_two_tab = (rgb_sample_pos + rgb_sample_neg) * 0.5;

    // Outer samples on the tab.
    vec3 rgb_sample_neg_outer = texture(diffuse_texture, vertex_uv + sampling_direction * (0.0/3.0 - 0.5)).rgb;
    vec3 rgb_sample_pos_outer = texture(diffuse_texture, vertex_uv + sampling_direction * (3.0/3.0 - 0.5)).rgb;

    vec3 rgb_four_tab = (rgb_sample_pos_outer + rgb_sample_neg_outer) * 0.25 + rgb_two_tab * 0.5;

    // Calculate luma for checking against the minimum and maximum value.
    float luma_four_tab = dot(rgb_four_tab, to_luma);

    if (luma_four_tab < luma_min || luma_four_tab > luma_max) {
        output_color = vec4(rgb_two_tab, 1.0);
    } else {
        output_color = vec4(rgb_four_tab, 1.0);
    }
}

#endif
