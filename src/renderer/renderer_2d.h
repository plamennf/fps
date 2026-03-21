#pragma once

#include "render_backend.h"

enum Flip_Mode {
    FLIP_MODE_NONE         = 0,
    FLIP_MODE_HORIZONTALLY = Bit(1),
    FLIP_MODE_VERTICALLY   = Bit(2),
    FLIP_MODE_BOTH         = FLIP_MODE_HORIZONTALLY | FLIP_MODE_VERTICALLY
};

struct Renderer_2D {
    struct Render_Quads_Entry {
        int num_quads;
        int index_array_offset;
        Texture *texture;
    };

    struct Per_Scene_Uniforms {
        glm::mat4 projection_matrix;
        glm::mat4 view_matrix;
    };
    
    Render_Backend *backend = NULL;
    
    const int MAX_QUADS = 20000;
    const int MAX_QUAD_VERTICES = MAX_QUADS * 4;
    const int MAX_QUAD_INDICES  = MAX_QUADS * 6;

    Immediate_Vertex *quad_vertices = NULL;
    int num_quad_vertices = 0;
    Gpu_Buffer quad_vertex_buffers[Render_Backend::MAX_FRAMES_IN_FLIGHT];
    
    u32 *quad_indices = NULL;
    int num_quad_indices = 0;
    Gpu_Buffer quad_index_buffers[Render_Backend::MAX_FRAMES_IN_FLIGHT];

    Render_Quads_Entry *render_quads = NULL;
    int num_render_quads = 0;
    Render_Quads_Entry *current_quads = NULL;

    int current_render_quads_offset = 0;
    
    VkDescriptorPool per_scene_descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout per_scene_descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorSet per_scene_descriptor_sets[Render_Backend::MAX_FRAMES_IN_FLIGHT];
    Gpu_Buffer per_scene_uniform_buffers[Render_Backend::MAX_FRAMES_IN_FLIGHT];
    
    VkDescriptorPool quad_descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout quad_descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorSet *quad_descriptor_sets = NULL;

    VkPipelineLayout quad_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline quad_pipeline = VK_NULL_HANDLE;

    VkPipeline pipeline_to_use = VK_NULL_HANDLE;
    Per_Scene_Uniforms per_scene_uniforms;
    
    bool init(Render_Backend *backend);
    void end_frame();
    
    void begin_2d(VkExtent2D extent, VkPipeline override_pipeline = VK_NULL_HANDLE, glm::mat4 view_matrix = glm::mat4(1.0f));
    void end_2d(VkCommandBuffer cb);

    void draw_quad(Texture *texture, glm::vec2 position, glm::vec2 size, Flip_Mode flip_mode, Rectangle2i *src_rect, glm::vec4 color);
    
private:
    Render_Quads_Entry *get_current_quads(Texture *texture, int num_quads);
};
