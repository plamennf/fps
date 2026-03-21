#include "pch.h"
#include "renderer_2d.h"

bool Renderer_2D::init(Render_Backend *_backend) {
    backend = _backend;

    //
    // Per scene descriptor vulkan objects
    //
    VkDescriptorPoolSize per_scene_pool_sizes[1] = {};

    per_scene_pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    per_scene_pool_sizes[0].descriptorCount = Render_Backend::MAX_FRAMES_IN_FLIGHT;
    
    if (!backend->create_descriptor_pool(&per_scene_descriptor_pool, ArrayCount(per_scene_pool_sizes), per_scene_pool_sizes, Render_Backend::MAX_FRAMES_IN_FLIGHT)) {
        return false;
    }

    VkDescriptorSetLayoutBinding per_scene_descriptor_set_layout_bindings[1] = {};

    per_scene_descriptor_set_layout_bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    per_scene_descriptor_set_layout_bindings[0].descriptorCount = 1;
    per_scene_descriptor_set_layout_bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    
    if (!backend->create_descriptor_set_layout(&per_scene_descriptor_set_layout, ArrayCount(per_scene_descriptor_set_layout_bindings), per_scene_descriptor_set_layout_bindings)) {
        return false;
    }
    
    if (!backend->create_descriptor_sets(per_scene_descriptor_pool, per_scene_descriptor_set_layout, Render_Backend::MAX_FRAMES_IN_FLIGHT, per_scene_descriptor_sets)) {
        return false;
    }

    for (int i = 0; i < Render_Backend::MAX_FRAMES_IN_FLIGHT; i++) {
        if (!backend->create_buffer(&per_scene_uniform_buffers[i], VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT , sizeof(Per_Scene_Uniforms), NULL)) {
            return false;
        }

        backend->update_descriptor_set(per_scene_descriptor_sets[i], 0, &per_scene_uniform_buffers[i]);
    }
    
    //
    // Quad descriptor vulkan objects
    //
    VkDescriptorPoolSize quad_pool_sizes[1] = {};

    quad_pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    quad_pool_sizes[0].descriptorCount = MAX_QUADS * Render_Backend::MAX_FRAMES_IN_FLIGHT;

    if (!backend->create_descriptor_pool(&quad_descriptor_pool, ArrayCount(quad_pool_sizes), quad_pool_sizes, MAX_QUADS * Render_Backend::MAX_FRAMES_IN_FLIGHT)) {
        return false;
    }
    
    VkDescriptorSetLayoutBinding quad_descriptor_set_layout_bindings[1] = {};

    quad_descriptor_set_layout_bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    quad_descriptor_set_layout_bindings[0].descriptorCount = 1;
    quad_descriptor_set_layout_bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    if (!backend->create_descriptor_set_layout(&quad_descriptor_set_layout, ArrayCount(quad_descriptor_set_layout_bindings), quad_descriptor_set_layout_bindings)) {
        return false;
    }

    quad_descriptor_sets = new VkDescriptorSet[MAX_QUADS * Render_Backend::MAX_FRAMES_IN_FLIGHT];
    if (!backend->create_descriptor_sets(quad_descriptor_pool, quad_descriptor_set_layout, MAX_QUADS * Render_Backend::MAX_FRAMES_IN_FLIGHT, quad_descriptor_sets)) {
        return false;
    }

    VkDescriptorSetLayout quad_pipeline_descriptor_set_layouts[] = {
        per_scene_descriptor_set_layout,
        quad_descriptor_set_layout,
    };

    if (!backend->create_graphics_pipeline_layout(ArrayCount(quad_pipeline_descriptor_set_layouts), quad_pipeline_descriptor_set_layouts, 0, NULL, &quad_pipeline_layout)) {
        return false;
    }

    Graphics_Pipeline_Info quad_pipepline_info = {};
    
    quad_pipepline_info.pipeline_layout = quad_pipeline_layout;
    quad_pipepline_info.shader_filename = "quad";
    quad_pipepline_info.vertex_type     = RENDER_VERTEX_IMMEDIATE;
    quad_pipepline_info.blend_mode      = BLEND_MODE_ALPHA;
    quad_pipepline_info.cull_mode       = CULL_MODE_NONE;
    quad_pipepline_info.depth_test_mode = DEPTH_TEST_MODE_OFF;
    quad_pipepline_info.depth_write     = false;
    quad_pipepline_info.color_write     = true;
    
    quad_pipepline_info.color_attachment_format = backend->get_swap_chain_surface_format();
    quad_pipepline_info.depth_attachment_format = VK_FORMAT_D32_SFLOAT;
    
    if (!backend->create_graphics_pipeline(quad_pipepline_info, &quad_pipeline)) {
        return false;
    }

    quad_vertices = new Immediate_Vertex[MAX_QUAD_VERTICES];
    quad_indices  = new u32[MAX_QUAD_INDICES];
    num_quad_vertices = 0;
    num_quad_indices  = 0;

    for (int i = 0; i < Render_Backend::MAX_FRAMES_IN_FLIGHT; i++) {
        if (!backend->create_buffer(&quad_vertex_buffers[i], VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, MAX_QUAD_VERTICES * sizeof(Immediate_Vertex), quad_vertices)) return false;
        if (!backend->create_buffer(&quad_index_buffers[i], VK_BUFFER_USAGE_INDEX_BUFFER_BIT, MAX_QUAD_INDICES * sizeof(u32), quad_indices)) return false;
    }
    
    render_quads = new Render_Quads_Entry[MAX_QUADS];
    num_render_quads = 0;
    current_quads = NULL;

    memset(&per_scene_uniforms, 0, sizeof(per_scene_uniforms));
    
    return true;
}

void Renderer_2D::begin_2d(VkExtent2D extent, VkPipeline override_pipeline, glm::mat4 view_matrix) {
    if (override_pipeline) {
        pipeline_to_use = override_pipeline;
    } else {
        pipeline_to_use = quad_pipeline;
    }

    per_scene_uniforms.projection_matrix = glm::ortho(0.0f, (float)extent.width, (float)extent.height, 0.0f, -1.0f, 1.0f);
    per_scene_uniforms.view_matrix = view_matrix;

    current_render_quads_offset = num_render_quads;
}

void Renderer_2D::end_frame() {
    num_render_quads = 0;
    current_quads = NULL;
    current_render_quads_offset = 0;

    num_quad_vertices = 0;
    num_quad_indices = 0;
}

void Renderer_2D::end_2d(VkCommandBuffer cb) {
    backend->update_buffer(&per_scene_uniform_buffers[backend->current_frame], 0, sizeof(per_scene_uniforms), &per_scene_uniforms);
    
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_to_use);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, quad_pipeline_layout, 0, 1, &per_scene_descriptor_sets[backend->current_frame], 0, NULL);

    backend->update_buffer(&quad_vertex_buffers[backend->current_frame], 0, num_quad_vertices * sizeof(Immediate_Vertex), quad_vertices);
    backend->update_buffer(&quad_index_buffers[backend->current_frame], 0, num_quad_indices * sizeof(u32), quad_indices);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &quad_vertex_buffers[backend->current_frame].buffer, &offset);
    vkCmdBindIndexBuffer(cb, quad_index_buffers[backend->current_frame].buffer, 0, VK_INDEX_TYPE_UINT32);
    
    for (int i = current_render_quads_offset; i < num_render_quads; i++) {
        Render_Quads_Entry *entry = &render_quads[i];

        VkDescriptorSet set = quad_descriptor_sets[i + (backend->current_frame * MAX_QUADS)];
        backend->update_descriptor_set(set, 0, entry->texture);
        
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, quad_pipeline_layout, 1, 1, &set, 0, NULL);
            
        vkCmdDrawIndexed(cb, entry->num_quads * 6, 1, entry->index_array_offset, 0, 0);
    }
}

Renderer_2D::Render_Quads_Entry *Renderer_2D::get_current_quads(Texture *texture, int num_quads) {
    Assert(num_quad_vertices + num_quads * 4 <= MAX_QUAD_VERTICES);
    Assert(num_quad_indices  + num_quads * 6 <= MAX_QUAD_INDICES);
    
    if (current_quads) {
        if (current_quads->texture != texture) {
            Assert(num_render_quads < MAX_QUADS);
            current_quads = NULL;
        }
    }

    if (!current_quads) {
        current_quads = &render_quads[num_render_quads++];
        current_quads->num_quads          = 0;
        current_quads->index_array_offset = num_quad_indices;
        current_quads->texture            = texture;
    }

    Assert(current_quads);
    return current_quads;
}

void Renderer_2D::draw_quad(Texture *texture, glm::vec2 position, glm::vec2 size, Flip_Mode flip_mode, Rectangle2i *src_rect, glm::vec4 color) {
    auto entry = get_current_quads(texture, 1);
    entry->num_quads++;
    
    glm::vec2 p0 = position;
    glm::vec2 p1 = glm::vec2(position.x + size.x, position.y);
    glm::vec2 p2 = position + size;
    glm::vec2 p3 = glm::vec2(position.x, position.y + size.y);

    glm::vec2 uv0, uv1, uv2, uv3;
    if (src_rect) {
        float min_uv_x = (float)src_rect->x / (float)texture->width;
        float min_uv_y = (float)src_rect->y / (float)texture->height;
        float max_uv_x = (float)(src_rect->x + src_rect->width)  / (float)texture->width;
        float max_uv_y = (float)(src_rect->y + src_rect->height) / (float)texture->height;

        uv0 = glm::vec2(min_uv_x, min_uv_y);
        uv1 = glm::vec2(max_uv_x, min_uv_y);
        uv2 = glm::vec2(max_uv_x, max_uv_y);
        uv3 = glm::vec2(min_uv_x, max_uv_y);
    } else {
        uv0 = glm::vec2(0, 0);
        uv1 = glm::vec2(1, 0);
        uv2 = glm::vec2(1, 1);
        uv3 = glm::vec2(0, 1);
    }

    if (flip_mode & FLIP_MODE_HORIZONTALLY) {
        float min_uv_x = uv0.x;
        float max_uv_x = uv1.x;

        uv0.x = max_uv_x;
        uv1.x = min_uv_x;
        uv2.x = min_uv_x;
        uv3.x = max_uv_x;
    }

    if (flip_mode & FLIP_MODE_VERTICALLY) {
        float min_uv_y = uv0.y;
        float max_uv_y = uv2.y;

        uv0.y = max_uv_y;
        uv1.y = max_uv_y;
        uv2.y = min_uv_y;
        uv3.y = min_uv_y;
    }
    
    quad_vertices[num_quad_vertices + 0].position = p0;
    quad_vertices[num_quad_vertices + 0].color    = color;
    quad_vertices[num_quad_vertices + 0].uv       = uv0;

    quad_vertices[num_quad_vertices + 1].position = p1;
    quad_vertices[num_quad_vertices + 1].color    = color;
    quad_vertices[num_quad_vertices + 1].uv       = uv1;

    quad_vertices[num_quad_vertices + 2].position = p2;
    quad_vertices[num_quad_vertices + 2].color    = color;
    quad_vertices[num_quad_vertices + 2].uv       = uv2;

    quad_vertices[num_quad_vertices + 3].position = p3;
    quad_vertices[num_quad_vertices + 3].color    = color;
    quad_vertices[num_quad_vertices + 3].uv       = uv3;

    quad_indices[num_quad_indices + 0] = num_quad_vertices + 0;
    quad_indices[num_quad_indices + 1] = num_quad_vertices + 1;
    quad_indices[num_quad_indices + 2] = num_quad_vertices + 2;
    quad_indices[num_quad_indices + 3] = num_quad_vertices + 0;
    quad_indices[num_quad_indices + 4] = num_quad_vertices + 2;
    quad_indices[num_quad_indices + 5] = num_quad_vertices + 3;

    num_quad_vertices += 4;
    num_quad_indices  += 6;
}
