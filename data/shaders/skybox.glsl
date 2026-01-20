OUT_IN vec3 v_uv;

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
    v_uv = input_position * 2.0;
    gl_Position = projection_matrix * mat4(mat3(view_matrix)) * world_matrix * vec4(input_position, 1.0);
    //gl_Position = projection_matrix * view_matrix * vec4(input_position, 1.0);
    gl_Position = gl_Position.xyww;
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) out vec4 output_color;

uniform samplerCube skybox;

void main(void) {
    output_color = texture(skybox, v_uv);
}

#endif
