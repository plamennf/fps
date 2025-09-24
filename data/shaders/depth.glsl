#ifdef VERTEX_SHADER

layout(location = 0) in vec3 input_position;
layout(location = 1) in vec2 input_uv;
layout(location = 2) in vec3 input_normal;

uniform mat4 light_matrix;
uniform mat4 world_matrix;

void main() {
    gl_Position = light_matrix * world_matrix * vec4(input_position, 1.0);
}

#endif

#ifdef FRAGMENT_SHADER

void main() {
    
}

#endif
