#include "main.h"
#include "renderer.h"
#include "font.h"
#include "mesh.h"

#include <GL/glew.h>
#include <float.h> // For FLT_MAX, FLT_MIN

const int MAX_IMMEDIATE_VERTICES = 2400;
static Immediate_Vertex immediate_vertices[MAX_IMMEDIATE_VERTICES];
static int num_immediate_vertices;

static Gpu_Buffer *immediate_vertex_buffer;

static Gpu_Buffer *cube_vertex_buffer;
static Gpu_Buffer *cube_index_buffer;

void immediate_init() {
    num_immediate_vertices = 0;
    immediate_vertex_buffer = make_gpu_buffer(GPU_BUFFER_VERTEX, MAX_IMMEDIATE_VERTICES * sizeof(Immediate_Vertex), NULL, true);
    
    Mesh_Vertex cube_vertices[] = {
        { { -0.5f, -0.5f, -0.5f }, { 0, 0 }, { 0, 0, -1 } },
        { { -0.5f, +0.5f, -0.5f }, { 0, 1 }, { 0, 0, -1 } },
        { { +0.5f, +0.5f, -0.5f }, { 1, 1 }, { 0, 0, -1 } },
        { { +0.5f, -0.5f, -0.5f }, { 1, 0 }, { 0, 0, -1 } },

        { { +0.5f, -0.5f, +0.5f }, { 0, 0 }, { 0, 0, +1 } },
        { { +0.5f, +0.5f, +0.5f }, { 0, 1 }, { 0, 0, +1 } },
        { { -0.5f, +0.5f, +0.5f }, { 1, 1 }, { 0, 0, +1 } },
        { { -0.5f, -0.5f, +0.5f }, { 1, 0 }, { 0, 0, +1 } },

        { { -0.5f, +0.5f, +0.5f }, { 0, 0 }, { -1, 0, 0 } },
        { { -0.5f, +0.5f, -0.5f }, { 0, 1 }, { -1, 0, 0 } },
        { { -0.5f, -0.5f, -0.5f }, { 1, 1 }, { -1, 0, 0 } },
        { { -0.5f, -0.5f, +0.5f }, { 1, 0 }, { -1, 0, 0 } },

        { { +0.5f, -0.5f, +0.5f }, { 0, 0 }, { +1, 0, 0 } },
        { { +0.5f, -0.5f, -0.5f }, { 0, 1 }, { +1, 0, 0 } },
        { { +0.5f, +0.5f, -0.5f }, { 1, 1 }, { +1, 0, 0 } },
        { { +0.5f, +0.5f, +0.5f }, { 1, 0 }, { +1, 0, 0 } },

        { { -0.5f, -0.5f, +0.5f }, { 0, 0 }, { 0, -1, 0 } },
        { { -0.5f, -0.5f, -0.5f }, { 0, 1 }, { 0, -1, 0 } },
        { { +0.5f, -0.5f, -0.5f }, { 1, 1 }, { 0, -1, 0 } },
        { { +0.5f, -0.5f, +0.5f }, { 1, 0 }, { 0, -1, 0 } },

        { { +0.5f, +0.5f, +0.5f }, { 0, 0 }, { 0, +1, 0 } },
        { { +0.5f, +0.5f, -0.5f }, { 0, 1 }, { 0, +1, 0 } },
        { { -0.5f, +0.5f, -0.5f }, { 1, 1 }, { 0, +1, 0 } },
        { { -0.5f, +0.5f, +0.5f }, { 1, 0 }, { 0, +1, 0 } },
    };

    u32 cube_indices[] = {
        // Back face (-Z)
        0, 1, 2,
        0, 2, 3,

        // Front face (+Z)
        4, 5, 6,
        4, 6, 7,

        // Left face (-X)
        8, 9, 10,
        8, 10, 11,

        // Right face (+X)
        12, 13, 14,
        12, 14, 15,

        // Bottom face (-Y)
        16, 17, 18,
        16, 18, 19,

        // Top face (+Y)
        20, 21, 22,
        20, 22, 23
    };

    cube_vertex_buffer = make_gpu_buffer(GPU_BUFFER_VERTEX, sizeof(cube_vertices), cube_vertices, false);
    cube_index_buffer = make_gpu_buffer(GPU_BUFFER_INDEX, sizeof(cube_indices), cube_indices, false);
}

void immediate_begin() {
    immediate_flush();
}

void immediate_flush() {
    if (!num_immediate_vertices) return;
    assert(get_current_shader());
    
    set_vertex_buffer(immediate_vertex_buffer);
    update_current_gpu_buffer(immediate_vertex_buffer, 0, num_immediate_vertices * sizeof(Immediate_Vertex), immediate_vertices);

    draw_non_indexed(num_immediate_vertices, 0);

    num_immediate_vertices = 0;
}

static void put_vertex(Immediate_Vertex *v, Vector2 position, Vector4 color, Vector2 uv) {
    v->position = position;
    v->color    = color;
    v->uv       = uv;
}

void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3, Vector4 color) {
    if (num_immediate_vertices + 6 > MAX_IMMEDIATE_VERTICES) immediate_flush();

    Immediate_Vertex *v = immediate_vertices + num_immediate_vertices;

    put_vertex(&v[0], p0, color, uv0);
    put_vertex(&v[1], p1, color, uv1);
    put_vertex(&v[2], p2, color, uv2);
    
    put_vertex(&v[3], p0, color, uv0);
    put_vertex(&v[4], p2, color, uv2);
    put_vertex(&v[5], p3, color, uv3);
    
    num_immediate_vertices += 6;
}

void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector4 color) {
    Vector2 uv0 = v2(0, 0);
    Vector2 uv1 = v2(1, 0);
    Vector2 uv2 = v2(1, 1);
    Vector2 uv3 = v2(0, 1);

    immediate_quad(p0, p1, p2, p3, uv0, uv1, uv2, uv3, color);
}

void immediate_quad(Vector2 position, Vector2 size, Vector4 color) {
    Vector2 p0 = position;
    Vector2 p1 = v2(position.x + size.x, position.y);
    Vector2 p2 = position + size;
    Vector2 p3 = v2(position.x, position.y + size.y);

    immediate_quad(p0, p1, p2, p3, color);
}

void rendering_2d() {
    float w = (float)globals.render_target_width;
    if (w < 1.0f) w = 1.0f;
    float h = (float)globals.render_target_height;
    if (h < 1.0f) h = 1.0f;

    globals.transform.projection_matrix = make_orthographic(0.0f, w, 0.0f, h, -1.0f, 1.0f);
    globals.transform.view_matrix       = matrix4_identity();
    globals.transform.world_matrix      = matrix4_identity();

    refresh_transform();

    set_blend_mode(BLEND_MODE_ALPHA);
    set_depth_test(DEPTH_TEST_OFF);
    set_cull_face(CULL_FACE_NONE);
    set_depth_write(false);
}

void rendering_3d() {
    float w = (float)globals.render_target_width;
    if (w < 1.0f) w = 1.0f;
    float h = (float)globals.render_target_height;
    if (h < 1.0f) h = 1.0f;
    
    float aspect_ratio = w / h;
    globals.transform.projection_matrix = make_perspective(aspect_ratio, CAMERA_FOV, CAMERA_Z_NEAR, CAMERA_Z_FAR);
    globals.transform.view_matrix       = get_view_matrix(&globals.camera);
    globals.transform.world_matrix      = matrix4_identity();
    
    refresh_transform();
    
    set_blend_mode(BLEND_MODE_OFF);
    set_depth_test(DEPTH_TEST_LESS);
    set_cull_face(CULL_FACE_BACK);
}

void rendering_3d_shadow_map() {
    float half_scene_size = 250.0f;
    Matrix4 light_projection = make_orthographic(-half_scene_size, half_scene_size, -half_scene_size, half_scene_size, 0.1f, 1000.0f);
    //Matrix4 light_view = make_look_at_matrix(globals.directional_light.direction * -500.0f, v3(0, 0, 0), v3(0, 1, 0));
    Matrix4 light_view = make_look_at_matrix(globals.directional_light.direction * -1.0f, v3(0, 0, 0), v3(0, 1, 0));
    globals.transform.light_matrix = light_projection * light_view;

    refresh_transform();

    set_blend_mode(BLEND_MODE_OFF);
    set_depth_test(DEPTH_TEST_LESS);
    set_cull_face(CULL_FACE_FRONT);

    set_shadow_map(globals.shadow_map_buffer);
}

void draw_text(Font *font, char *text, int x, int y, Vector4 color) {
    set_shader(globals.shader_texture);
    set_texture(TEXTURE_DIFFUSE, font->texture);

    immediate_begin();
    for (char *at = text; *at; at++) {
        Glyph *glyph = &font->glyphs[*at];
        if (!glyph) continue;

        if (*at == '\n') {
            logprintf("Reached new line in draw_text: Stopping drawing!\n");
            break;
        }

        if (!is_space(*at)) {
            Vector2 position;
            position.x = (float)(x + glyph->bearing_x);
            position.y = (float)(y - (glyph->size_y - glyph->bearing_y));

            Vector2 size;
            size.x = (float)glyph->size_x;
            size.y = (float)glyph->size_y;

            float min_uv_x = (float)glyph->src_rect.x / (float)font->texture->width;
            float min_uv_y = (float)glyph->src_rect.y / (float)font->texture->height;
            float max_uv_x = (float)(glyph->src_rect.x + glyph->src_rect.width) / (float)font->texture->width;
            float max_uv_y = (float)(glyph->src_rect.y + glyph->src_rect.height) / (float)font->texture->height;
            
            Vector2 p0 = position;
            Vector2 p1 = v2(position.x + size.x, position.y);
            Vector2 p2 = position + size;
            Vector2 p3 = v2(position.x, position.y + size.y);

            Vector2 uv0 = v2(min_uv_x, max_uv_y);
            Vector2 uv1 = v2(max_uv_x, max_uv_y);
            Vector2 uv2 = v2(max_uv_x, min_uv_y);
            Vector2 uv3 = v2(min_uv_x, min_uv_y);
            
            immediate_quad(p0, p1, p2, p3, uv0, uv1, uv2, uv3, color);
        }

        x += glyph->advance;
    }
    immediate_flush();
}

void draw_mesh(Mesh *mesh, Vector3 position, Vector3 rotation, float scale) {
    if (globals.render_stage == RENDER_STAGE_MAIN) {
        set_shader(globals.shader_basic);
    } else {
        set_shader(globals.shader_depth);
    }

    globals.transform.world_matrix = make_transformation_matrix(position, rotation, scale);
    refresh_transform();
    
    for (int i = 0; i < mesh->num_submeshes; i++) {
        Submesh *submesh = &mesh->submeshes[i];

        set_vertex_buffer(submesh->vertex_buffer);
        set_index_buffer(submesh->index_buffer);

        set_texture(TEXTURE_DIFFUSE,  submesh->material.diffuse_texture);
        set_texture(TEXTURE_SPECULAR, submesh->material.specular_texture);
        if (submesh->material.normal_texture) {
            set_texture(TEXTURE_NORMAL, submesh->material.normal_texture);
        }
        
        refresh_material(&submesh->material);
        
        draw_indexed(submesh->num_indices, 0);
    }
}

void draw_cube(Vector3 position, Vector3 rotation, Vector3 scale, Vector4 color) {
    if (globals.render_stage == RENDER_STAGE_MAIN) {
        set_shader(globals.shader_basic);
    } else {
        set_shader(globals.shader_depth);
    }

    globals.transform.world_matrix = make_transformation_matrix(position, rotation, scale);
    refresh_transform();
    
    set_vertex_buffer(cube_vertex_buffer);
    set_index_buffer(cube_index_buffer);

    set_texture(TEXTURE_DIFFUSE, globals.white_texture);
    set_texture(TEXTURE_SPECULAR, globals.black_texture);

    Material material = {};
    material.diffuse_color  = color;
    material.shininess      = 1.0f;
    refresh_material(&material);

    draw_indexed(36, 0);
}
