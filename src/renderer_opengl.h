#pragma once

#include "renderer.h"

#include <GL/glew.h>

const int MAX_FRAMEBUFFERS = 1024;
const int MAX_GPU_BUFFERS = 1024 * 10 * 2;
const int MAX_SHADERS = 512;
const int MAX_TEXTURES = 1024;

struct Framebuffer_Gl : public Framebuffer {
    GLuint fbo_id;
    GLuint color_id;
    GLuint depth_id;
};

struct Gpu_Buffer_Gl : public Gpu_Buffer {
    GLuint id;
};

struct Shader_Gl : public Shader {
    GLuint program_id;

    GLint diffuse_texture;
    GLint specular_texture;
    GLint normal_texture;

    GLint projection_matrix;
    GLint view_matrix;
    GLint world_matrix;
    
    GLint material_color;
    GLint material_shininess;
    GLint material_use_normal_map;
    
    GLint camera_position;
    
    GLint directional_light_direction;
    GLint directional_light_ambient;
    GLint directional_light_diffuse;
    GLint directional_light_specular;

    GLint point_light_position[MAX_POINT_LIGHTS];
    GLint point_light_constant[MAX_POINT_LIGHTS];
    GLint point_light_linear[MAX_POINT_LIGHTS];
    GLint point_light_quadratic[MAX_POINT_LIGHTS];
    GLint point_light_ambient[MAX_POINT_LIGHTS];
    GLint point_light_diffuse[MAX_POINT_LIGHTS];
    GLint point_light_specular[MAX_POINT_LIGHTS];

    GLint spot_light_position;
    GLint spot_light_direction;
    GLint spot_light_cut_off;
    GLint spot_light_outer_cut_off;
    GLint spot_light_ambient;
    GLint spot_light_diffuse;
    GLint spot_light_specular;
    GLint spot_light_constant;
    GLint spot_light_linear;
    GLint spot_light_quadratic;
    
    GLint num_multisamples;
};

struct Texture_Gl : public Texture {
    GLenum internal_format;
    GLenum source_format;
    
    GLuint id;
};
