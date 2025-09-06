OUT_IN vec4 vertex_color;

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
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) out vec4 output_color;

void main(void) {
    output_color = vertex_color;
}

#endif
