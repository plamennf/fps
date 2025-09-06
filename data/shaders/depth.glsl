#ifdef VERTEX_SHADER

layout(location = 0) in vec3 input_position;
layout(location = 1) in vec2 input_uv;
layout(location = 2) in vec3 input_normal;

uniform mat4 world_matrix;

void main() {
    gl_Position = world_matrix * vec4(input_position, 1.0);
}

#endif

#ifdef GEOMETRY_SHADER

layout(triangles, invocations = 5) in;
layout(triangle_strip, max_vertices = 3) out;

layout(std140, binding = 0) uniform Light_Space_Matrices {
    mat4 light_space_matrices[16];
};

void main() {
    for (int i = 0; i < 3; i++) {
        gl_Position = light_space_matrices[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}

#endif

#ifdef FRAGMENT_SHADER

void main() {
    
}

#endif
