#pragma once

#include "geometry.h"

struct Font;
struct Material;

const int MAX_POINT_LIGHTS = 4;

struct Directional_Light {
    Vector3 direction;

    Vector3 ambient;
    Vector3 diffuse;
    Vector3 specular;
};

struct Point_Light {
    Vector3 position;

    float constant;
    float linear;
    float quadratic;

    Vector3 ambient;
    Vector3 diffuse;
    Vector3 specular;
};

struct Spot_Light {
    Vector3 position;
    Vector3 direction;
    float cut_off;
    float outer_cut_off;

    float constant;
    float linear;
    float quadratic;

    Vector3 ambient;
    Vector3 diffuse;
    Vector3 specular;
};

enum Blend_Mode {
    BLEND_MODE_OFF,
    BLEND_MODE_ALPHA,
};

enum Depth_Test_Mode {
    DEPTH_TEST_OFF,
    DEPTH_TEST_LEQUAL,
    DEPTH_TEST_LESS,
};

enum Cull_Face {
    CULL_FACE_NONE,
    CULL_FACE_BACK,
    CULL_FACE_FRONT,
};

enum Texture_Format {
    TEXTURE_FORMAT_UNKNOWN,
    TEXTURE_FORMAT_RGBA8,
    TEXTURE_FORMAT_RGB8,
    TEXTURE_FORMAT_RG8,
    TEXTURE_FORMAT_R8,

    TEXTURE_FORMAT_D24S8,
    TEXTURE_FORMAT_SHADOW_MAP,
};

enum Texture_Type {
    TEXTURE_DIFFUSE,
    TEXTURE_SPECULAR,
    TEXTURE_NORMAL,
    //TEXTURE_METALLIC_ROUGHNESS,
    TEXTURE_TYPE_COUNT,
};

struct Texture {
    char *filepath;
    
    int width;
    int height;

    Texture_Format format;
    int bpp;
};

struct Framebuffer {
    int width;
    int height;

    Texture_Format color_format;
    Texture_Format depth_format;
};

enum Gpu_Buffer_Type {
    GPU_BUFFER_VERTEX,
    GPU_BUFFER_INDEX,
};

struct Gpu_Buffer {
    Gpu_Buffer_Type type;
    u32 size;
    bool is_dynamic;
};

enum Render_Vertex_Type {
    RENDER_VERTEX_UNKNOWN,
    RENDER_VERTEX_MESH,
    RENDER_VERTEX_IMMEDIATE,
};

struct Mesh_Vertex {
    Vector3 position;
    Vector2 uv;
    Vector3 normal;
    Vector3 tangent;
    Vector3 bitangent;
};

struct Immediate_Vertex {
    Vector2 position;
    Vector4 color;
    Vector2 uv;
};

struct Shader {
    Render_Vertex_Type vertex_type;
};

struct Transform {
    //Matrix4 wvp_matrix;
    Matrix4 projection_matrix;
    Matrix4 view_matrix;
    Matrix4 world_matrix;
    Matrix4 light_matrix;
};

void init_renderer(struct SDL_Window *window);
void swap_buffers();

void set_blend_mode(Blend_Mode blend_mode);
void set_depth_test(Depth_Test_Mode depth_test_mode);
void set_cull_face(Cull_Face cull_face);
void set_depth_write(bool write);

void draw_indexed(u32 num_indices, u32 first_index);
void draw_non_indexed(u32 num_vertices, u32 first_vertex);

Texture *make_texture(int width, int height, Texture_Format format, u8 *data, char *filepath = NULL);
Texture *load_texture(char *filepath);
Texture *load_cubemap(char *filepaths_of_faces[6]);
void destroy_texture(Texture *texture);
void set_texture(Texture_Type type, Texture *texture);
void set_color_texture(Framebuffer *framebuffer);
void set_shadow_map(Framebuffer *framebuffer, int index);
void set_cube_map(Texture *texture);

Framebuffer *make_framebuffer(int width, int height, Texture_Format color_format, Texture_Format depth_format);
Framebuffer *make_multisampled_framebuffer(int width, int height, Texture_Format color_format, Texture_Format depth_format, int num_multisamples);
void destroy_framebuffer(Framebuffer *framebuffer);
void set_framebuffer(Framebuffer *framebuffer, bool clear_color, Vector4 color, bool clear_depth, float z, bool clear_stencil, u8 stencil);

Gpu_Buffer *make_gpu_buffer(Gpu_Buffer_Type type, u32 size, void *data, bool is_dynamic);
void set_vertex_buffer(Gpu_Buffer *vertex_buffer);
void set_index_buffer(Gpu_Buffer *index_buffer);
void update_current_gpu_buffer(Gpu_Buffer *buffer, u32 offset, u32 size, void *data);

Shader *load_shader(char *filepath, Render_Vertex_Type vertex_type);
void set_shader(Shader *shader);
Shader *get_current_shader();

void refresh_transform();
void refresh_material(Material *material);
void refresh_lights();
void refresh_csm();

void immediate_begin();
void immediate_flush();
void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3, Vector4 color);
void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector4 color);
void immediate_quad(Vector2 position, Vector2 size, Vector4 color);

void rendering_2d();
void rendering_3d();
void rendering_3d_shadow_map(int cascade_index);

void draw_text(Font *font, char *text, int x, int y, Vector4 color);

struct Mesh;
void draw_mesh(Mesh *mesh, Vector3 position, Quaternion rotation, Vector3 scale, Vector4 scale_color);

void draw_cube(Vector3 position, Quaternion rotation, Vector3 scale, Vector4 color);
