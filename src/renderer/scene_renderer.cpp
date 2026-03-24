#include "pch.h"
#include "scene_renderer.h"
#include "renderer_2d.h"
#include "mesh.h"
#include "../main.h"
#include "texture_registry.h"
#include "mesh_registry.h"
#include "../terrain.h"

bool Scene_Renderer::init(Render_Backend *_backend, Renderer_2D *_renderer_2d) {
    backend = _backend;
    renderer_2d = _renderer_2d;
    
    if (!create_per_scene_vulkan_objects())       return false;
    if (!create_material_vulkan_objects())        return false;
    if (!create_fullscreen_quad_vulkan_objects()) return false;
    if (!create_pipeline_layouts())               return false;
    if (!create_pipelines())                      return false;

    if (!init_framebuffers()) {
        return false;
    }

    glm::vec4 noise_data[16];
    for (int i = 0; i < 16; i++) {
        float x = random_float(-1.0f, 1.0f);
        float y = random_float(-1.0f, 1.0f);

        glm::vec3 v = glm::normalize_or_zero(glm::vec3(x, y, 0.0f));

        noise_data[i] = glm::vec4(v, 0.0f);
    }

    if (!backend->create_texture(&ssao_noise_texture, 4, 4, VK_FORMAT_R16G16B16A16_SFLOAT, noise_data)) return false;
    
    return true;
}

void Scene_Renderer::destroy_framebuffers() {
    if (depth_buffer.image) {
        backend->destroy_texture(&depth_buffer);
    }

    if (position_buffer.image) {
        backend->destroy_texture(&position_buffer);
    }

    if (normal_buffer.image) {
        backend->destroy_texture(&normal_buffer);
    }
    
    if (offscreen_buffer.image) {
        backend->destroy_texture(&offscreen_buffer);
    }

    if (ssao_buffer.image) {
        backend->destroy_texture(&ssao_buffer);
    }

    if (ssao_blur_buffer.image) {
        backend->destroy_texture(&ssao_blur_buffer);
    }
    
    for (int i = 0; i < MAX_SHADOW_CASCADES; i++) {
        if (shadow_map_buffers[i].image) {
            backend->destroy_texture(&shadow_map_buffers[i]);
        }
    }
}

bool Scene_Renderer::init_framebuffers() {
    if (!backend->create_framebuffer(&depth_buffer, backend->get_swap_chain_extent().width, backend->get_swap_chain_extent().height, VK_FORMAT_D32_SFLOAT)) {
        return false;
    }

    if (!backend->create_framebuffer(&position_buffer, backend->get_swap_chain_extent().width, backend->get_swap_chain_extent().height, VK_FORMAT_R16G16B16A16_SFLOAT)) {
        return false;
    }

    if (!backend->create_framebuffer(&normal_buffer, backend->get_swap_chain_extent().width, backend->get_swap_chain_extent().height, VK_FORMAT_R16G16B16A16_SFLOAT)) {
        return false;
    }
    
    if (!backend->create_framebuffer(&offscreen_buffer, backend->get_swap_chain_extent().width, backend->get_swap_chain_extent().height, VK_FORMAT_R16G16B16A16_SFLOAT)) {
        return false;
    }

    if (!backend->create_framebuffer(&ssao_buffer, backend->get_swap_chain_extent().width, backend->get_swap_chain_extent().height, VK_FORMAT_R16_SFLOAT)) {
        return false;
    }

    if (!backend->create_framebuffer(&ssao_blur_buffer, backend->get_swap_chain_extent().width, backend->get_swap_chain_extent().height, VK_FORMAT_R16_SFLOAT)) {
        return false;
    }
    
    for (int i = 0; i < MAX_SHADOW_CASCADES; i++) {
        if (!backend->create_framebuffer(&shadow_map_buffers[i], SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, VK_FORMAT_D32_SFLOAT)) {
            return false;
        }

        for (int j = 0; j < Render_Backend::MAX_FRAMES_IN_FLIGHT; j++) {
            backend->update_descriptor_set(per_scene_uniforms_descriptor_sets[j], i + 1, &shadow_map_buffers[i]);
        }
    }

    for (int j = 0; j < Render_Backend::MAX_FRAMES_IN_FLIGHT; j++) {
        backend->update_descriptor_set(per_scene_uniforms_descriptor_sets[j], MAX_SHADOW_CASCADES + 1, &ssao_blur_buffer);
    }
    
    return true;
}

void Scene_Renderer::render() {
    MyZoneScopedN("Scene_Renderer::render");
    
    VkCommandBuffer cb = backend->get_current_command_buffer(false);
    
    VkExtent2D extent = backend->get_swap_chain_extent();

    float aspect_ratio = extent.width / (extent.height > 0 ? (float)extent.height : 1.0f);
    glm::mat4 projection_matrix = glm::perspective(glm::radians(camera.fov), aspect_ratio, camera.z_near, camera.z_far);
    projection_matrix[1][1] *= -1;
    glm::mat4 view_matrix = get_view_matrix(&camera);

    Per_Scene_Uniforms per_scene_uniforms;
    per_scene_uniforms.projection_matrix = projection_matrix;
    per_scene_uniforms.view_matrix       = view_matrix;

    //per_scene_uniforms.ssao_radius       = 0.5f;
    per_scene_uniforms.ssao_radius       = 5.0f;
    //per_scene_uniforms.ssao_bias         = 0.025f;
    per_scene_uniforms.ssao_bias         = 0.005f;
    per_scene_uniforms.ssao_noise_scale  = glm::vec2(extent.width / 4.0f, extent.height / 4.0f);
    
    memcpy(per_scene_uniforms.lights, lights, MAX_LIGHTS * sizeof(Light));

    per_scene_uniforms.camera_position   = camera.position;

    {
        Light *directional_light = &lights[0];
        for (int i = 1; i < num_lights; i++) {
            if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
                directional_light = &lights[i];
                break;
            }
        }

        update_shadow_map_cascade_matrices(&per_scene_uniforms, directional_light);
    }
    
    backend->update_buffer(&per_scene_uniform_buffers[backend->current_frame], 0, sizeof(per_scene_uniforms), &per_scene_uniforms);
    
    current_render_stage = RENDER_STAGE_SHADOWS;
    
    for (int i = 0; i < MAX_SHADOW_CASCADES; i++) {
        begin_rendering(cb, 0, NULL, &shadow_map_buffers[i], {SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT}, NULL, 1.0f, 0);

        render_all_entities(cb, i);

        end_rendering(cb);

        backend->image_layout_transition(cb, shadow_map_buffers[i].image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });
    }

    // Draw z-prepass
    Texture framebuffers[] = { normal_buffer, position_buffer };
    glm::vec4 clear_colors[] = { glm::vec4(0.0f), glm::vec4(0.0f) };
    begin_rendering(cb, 2, framebuffers, &depth_buffer, extent, clear_colors, 1.0f, 0);
    current_render_stage = RENDER_STAGE_Z_PREPASS;
    render_all_entities(cb);
    end_rendering(cb);

    backend->image_layout_transition(cb, position_buffer.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    backend->image_layout_transition(cb, normal_buffer.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    backend->image_layout_transition(cb, depth_buffer.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });

    // Draw ssao passes

    VkDescriptorSet descriptor_set = fullscreen_quad_descriptor_sets[backend->current_frame * MAX_RESOLVE_PASSES];
    backend->update_descriptor_set(descriptor_set, 1, &position_buffer);
    backend->update_descriptor_set(descriptor_set, 2, &normal_buffer);
    backend->update_descriptor_set(descriptor_set, 3, &depth_buffer);
    backend->update_descriptor_set(descriptor_set, 4, &ssao_noise_texture);
    glm::vec4 ssao_clear_color = glm::vec4(0);
    begin_rendering(cb, 1, &ssao_buffer, NULL, extent, &ssao_clear_color, 1.0f, 0);
    draw_fullscreen_quad(cb, ssao_pipeline, descriptor_set);
    end_rendering(cb);
    backend->image_layout_transition(cb, ssao_buffer.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    
    descriptor_set = fullscreen_quad_descriptor_sets[backend->current_frame * MAX_RESOLVE_PASSES + 1];
    backend->update_descriptor_set(descriptor_set, 1, &ssao_buffer);
    
    begin_rendering(cb, 1, &ssao_blur_buffer, NULL, extent, &ssao_clear_color, 1.0f, 0);
    draw_fullscreen_quad(cb, ssao_blur_pipeline, descriptor_set);
    end_rendering(cb);
    backend->image_layout_transition(cb, ssao_blur_buffer.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    
    // Draw main pass
    glm::vec4 offscreen_buffer_clear_color = glm::vec4(0.2f, 0.5f, 0.8f, 1.0f);
    begin_rendering(cb, 1, &offscreen_buffer, &depth_buffer, extent, &offscreen_buffer_clear_color, -1.0f, 0);
    current_render_stage = RENDER_STAGE_MAIN;
    render_all_entities(cb);
    end_rendering(cb);

    backend->image_layout_transition(cb, offscreen_buffer.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    
    Texture back_buffer   = backend->get_current_back_buffer();
    glm::vec4 clear_color = glm::vec4(0, 0, 0, 1);
    begin_rendering(cb, 1, &back_buffer, NULL, extent, &clear_color, 1.0f, 0);

    descriptor_set = fullscreen_quad_descriptor_sets[backend->current_frame * MAX_RESOLVE_PASSES + 2];
    backend->update_descriptor_set(descriptor_set, 1, &offscreen_buffer);
    draw_fullscreen_quad(cb, resolve_pipeline, descriptor_set);

    /*
    renderer_2d->begin_2d(extent, resolve_pipeline);
    renderer_2d->draw_quad(&offscreen_buffer, {0, 0}, {(float)extent.width, (float)extent.height}, FLIP_MODE_VERTICALLY, NULL, glm::vec4(1, 1, 1, 1));
    renderer_2d->end_2d(cb);
    */
    
    renderer_2d->begin_2d(extent);
    draw_hud(extent);
    renderer_2d->end_2d(cb);

    globals.render_backend->imgui_begin_frame();
    draw_imgui_stuff();
    globals.render_backend->imgui_end_frame(cb);
    
    end_rendering(cb);

    backend->image_layout_transition(cb, backend->get_current_swap_chain_image(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    render_entities.clear();
    terrain_chunks.clear();
    num_lights = 0;
}

void Scene_Renderer::set_camera(Camera _camera) {
    camera = _camera;

    shadow_cascade_splits[MAX_SHADOW_CASCADES - 1] = camera.z_far;
}

void Scene_Renderer::add_light(Light light) {
    if (num_lights >= MAX_LIGHTS) {
        logprintf("You are trying to add too many lights! The max is: %d\n", MAX_LIGHTS);
        return;
    }

    lights[num_lights++] = light;
}

void Scene_Renderer::add_render_entity(Mesh *mesh, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 scale_color) {
    glm::mat4 world = glm::mat4(1.0f);
    world = glm::translate(world, position);
    world = glm::rotate(world, rotation.x, glm::vec3(1, 0, 0));
    world = glm::rotate(world, rotation.y, glm::vec3(0, 1, 0));
    world = glm::rotate(world, rotation.z, glm::vec3(0, 0, 1));
    world = glm::scale(world, scale);

    Render_Entity entity;

    entity.world_matrix = world;
    entity.mesh         = mesh;
    entity.scale_color  = scale_color;
    
    render_entities.push_back(entity);
}

void Scene_Renderer::add_terrain_chunk(Terrain_Chunk *chunk) {
    terrain_chunks.push_back(*chunk);
}

void Scene_Renderer::generate_gpu_data_for_submesh(Submesh *submesh) {
    MyZoneScopedN("Generate gpu data for submesh");
    
    if (!backend->create_buffer(&submesh->vertex_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, submesh->num_vertices * sizeof(Mesh_Vertex), submesh->vertices)) return;
    if (!backend->create_buffer(&submesh->index_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, submesh->num_indices * sizeof(u32), submesh->indices)) return;
    
    submesh->material.albedo_texture = globals.white_texture;
    if (submesh->material.albedo_texture_name) {
        submesh->material.albedo_texture = globals.texture_registry->find_or_load(submesh->material.albedo_texture_name);
    }

    submesh->material.normal_texture = globals.white_texture;
    if (submesh->material.normal_texture_name) {
        submesh->material.normal_texture = globals.texture_registry->find_or_load(submesh->material.normal_texture_name);
    }

    submesh->material.metallic_roughness_texture = globals.white_texture;
    if (submesh->material.metallic_roughness_texture_name) {
        submesh->material.metallic_roughness_texture = globals.texture_registry->find_or_load(submesh->material.metallic_roughness_texture_name);
    }

    submesh->material.ao_texture = globals.white_texture;
    if (submesh->material.ao_texture_name) {
        submesh->material.ao_texture = globals.texture_registry->find_or_load(submesh->material.ao_texture_name);
    }

    submesh->material.emissive_texture = globals.white_texture;
    if (submesh->material.emissive_texture_name) {
        submesh->material.emissive_texture = globals.texture_registry->find_or_load(submesh->material.emissive_texture_name);
    }

    Material_Uniforms material_uniforms;

    material_uniforms.albedo_factor   = submesh->material.albedo_factor;
    material_uniforms.has_normal_map  = submesh->material.normal_texture && submesh->material.normal_texture != globals.white_texture;
    material_uniforms.emissive_factor = submesh->material.emissive_factor;
    material_uniforms.uses_specular_glossiness = submesh->material.uses_specular_glossiness ? 1 : 0;
    
    if (!backend->create_buffer(&submesh->material.uniform_buffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Material_Uniforms), &material_uniforms)) return;
    
    if (!backend->create_descriptor_sets(material_uniforms_descriptor_pool, material_uniforms_descriptor_set_layout, 1, &submesh->material.descriptor_set)) return;
    
    backend->update_descriptor_set(submesh->material.descriptor_set, 0, &submesh->material.uniform_buffer);
    backend->update_descriptor_set(submesh->material.descriptor_set, 1, submesh->material.albedo_texture);
    backend->update_descriptor_set(submesh->material.descriptor_set, 2, submesh->material.normal_texture);
    backend->update_descriptor_set(submesh->material.descriptor_set, 3, submesh->material.metallic_roughness_texture);
    backend->update_descriptor_set(submesh->material.descriptor_set, 4, submesh->material.ao_texture);
    backend->update_descriptor_set(submesh->material.descriptor_set, 5, submesh->material.emissive_texture);

    submesh->has_gpu_data = true;
}

void Scene_Renderer::render_all_entities(VkCommandBuffer cb, int cascade_index) {
    //
    // Draw instanced objects
    //
    
    for (int i = 0; i < terrain_chunks.size(); i++) {
        MyZoneScopedN("Render one terrain chunk");
        
        auto &chunk = terrain_chunks[i];

        {
            MyZoneScopedN("Render terrain mesh");
            
            switch (current_render_stage) {
                case RENDER_STAGE_Z_PREPASS: {
                    pipeline_for_current_pass = z_prepass_pipeline;
                } break;
                
                case RENDER_STAGE_SHADOWS: {
                    pipeline_for_current_pass = shadow_pipeline;
                } break;

                case RENDER_STAGE_MAIN: {
                    pipeline_for_current_pass = mesh_pipeline;
                } break;
            }

            pipeline_layout_for_current_pass = mesh_pipeline_layout;

            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_for_current_pass);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_for_current_pass, 0, 1, &per_scene_uniforms_descriptor_sets[backend->current_frame], 0, NULL);
            
            glm::vec3 world_position = chunk.offset;
            glm::mat4 world_matrix = glm::translate(glm::mat4(1.0f), world_position);
            draw_mesh(cb, (Mesh *)&chunk.mesh, world_matrix, glm::vec4(0, 1, 0, 1), cascade_index);
        }

        if (current_render_stage != RENDER_STAGE_Z_PREPASS) {
            int instance_buffer_index = MAX_RENDER_PASSES * backend->current_frame;
            switch (current_render_stage) {
                case RENDER_STAGE_SHADOWS: {
                    pipeline_for_current_pass = shadow_instanced_pipeline;
                    instance_buffer_index += cascade_index;
                } break;

                case RENDER_STAGE_MAIN: {
                    pipeline_for_current_pass = mesh_instanced_pipeline;
                    instance_buffer_index += MAX_SHADOW_CASCADES;
                } break;
            }
    
            pipeline_layout_for_current_pass = mesh_instanced_pipeline_layout;

            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_for_current_pass);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_for_current_pass, 0, 1, &per_scene_uniforms_descriptor_sets[backend->current_frame], 0, NULL);

            for (auto &batch : chunk.batches) {
                MyZoneScopedN("Update data for terrain chunk batches");
            
                for (int i = batch.start_index; i < batch.start_index + batch.count; i++) {
                    auto &object = chunk.objects[i];

                    chunk.objects_instance_data[i].shadow_cascade_index = cascade_index;
                }
            }

            Gpu_Buffer *instance_buffer = &chunk.instance_buffers[instance_buffer_index];
        
            {
                MyZoneScopedN("Update instance buffer");
                backend->update_buffer(instance_buffer, 0, chunk.objects_instance_data.size() * sizeof(Per_Object_Uniforms), chunk.objects_instance_data.data());
            }
            
            for (auto const &batch : chunk.batches) {
                MyZoneScopedN("Render one terrain chunk batch");
                draw_mesh_instanced(cb, batch.mesh, instance_buffer, batch.start_index, batch.count);
            }
        }
    }

    //
    // Draw non-instanced objects
    //
    
    switch (current_render_stage) {
        case RENDER_STAGE_Z_PREPASS: {
            pipeline_for_current_pass = z_prepass_pipeline;
        } break;
        
        case RENDER_STAGE_SHADOWS: {
            pipeline_for_current_pass = shadow_pipeline;
        } break;

        case RENDER_STAGE_MAIN: {
            pipeline_for_current_pass = mesh_pipeline;
        } break;
    }

    pipeline_layout_for_current_pass = mesh_pipeline_layout;

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_for_current_pass);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_for_current_pass, 0, 1, &per_scene_uniforms_descriptor_sets[backend->current_frame], 0, NULL);
    
    for (int i = 0; i < render_entities.size(); i++) {
        MyZoneScopedN("Draw single mesh");
        
        auto const &entity = render_entities[i];
        draw_mesh(cb, entity.mesh, entity.world_matrix, entity.scale_color, cascade_index);
    }
}

void Scene_Renderer::begin_rendering(VkCommandBuffer cb, int num_color_targets, Texture *color_targets, Texture *depth_target, VkExtent2D extent, glm::vec4 *_clear_colors, float z, u8 stencil) {
    VkClearValue clear_colors[MAX_COLOR_TARGETS] = {};
    for (int i = 0; i < num_color_targets; i++) {
        clear_colors[i].color.float32[0] = _clear_colors[i].r;
        clear_colors[i].color.float32[1] = _clear_colors[i].g;
        clear_colors[i].color.float32[2] = _clear_colors[i].b;
        clear_colors[i].color.float32[3] = _clear_colors[i].a;
    }
        
    VkClearValue clear_depth;
    clear_depth.depthStencil.depth   = z;
    clear_depth.depthStencil.stencil = stencil;
    
    VkRenderingAttachmentInfoKHR color_attachment_infos[MAX_COLOR_TARGETS] = {}, depth_attachment_info = {};
    if (color_targets && num_color_targets > 0) {
        for (int i = 0; i < num_color_targets; i++) {
            Texture *color_target = &color_targets[i];
            
            backend->image_layout_transition(cb, color_target->image, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
            
            color_attachment_infos[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            //color_attachment_info.imageView = backend->get_current_swap_chain_image_view();
            color_attachment_infos[i].imageView   = color_target->view;
            color_attachment_infos[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color_attachment_infos[i].resolveMode = VK_RESOLVE_MODE_NONE;
            color_attachment_infos[i].loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_attachment_infos[i].storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
            color_attachment_infos[i].clearValue  = clear_colors[i];
        }
    }
    
    if (depth_target) {
        VkImageLayout layout_from = VK_IMAGE_LAYOUT_UNDEFINED;
        if (z < 0.0f) {
            layout_from = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        
        backend->image_layout_transition(cb, depth_target->image, layout_from, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });
        
        depth_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment_info.imageView = depth_target->view;
        depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_attachment_info.loadOp = z >= 0.0f ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment_info.clearValue = clear_depth;
    }

    VkRenderingInfoKHR rendering_info = {};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    rendering_info.renderArea.offset = {0, 0};
    rendering_info.renderArea.extent = extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = num_color_targets;
    rendering_info.pColorAttachments = (num_color_targets > 0 && color_targets) ? color_attachment_infos : NULL;
    rendering_info.pDepthAttachment = depth_target ? &depth_attachment_info : NULL;
    vkCmdBeginRendering(cb, &rendering_info);
    
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cb, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cb, 0, 1, &scissor);
}

void Scene_Renderer::end_rendering(VkCommandBuffer cb) {
    vkCmdEndRendering(cb);
}

void Scene_Renderer::update_shadow_map_cascade_matrices(Per_Scene_Uniforms *uniforms, Light *directional_light) {
    glm::mat4 view_matrix = get_view_matrix(&camera);
    glm::mat4 inv_view_matrix = glm::inverse(view_matrix);

    glm::mat4 light_matrix = glm::lookAt(glm::vec3(0, 0, 0), directional_light->direction, glm::vec3(0, 1, 0));

    VkExtent2D extent = backend->get_swap_chain_extent();
    float aspect_ratio = (float)extent.width / (float)extent.height;
    float tan_half_v_fov = tanf(glm::radians(camera.fov * 0.5f));
    float tan_half_h_fov = tan_half_v_fov * aspect_ratio;

    for (int i = 0; i < MAX_SHADOW_CASCADES; i++) {
        float curr_cascade = (i == 0) ? camera.z_near : shadow_cascade_splits[i - 1];
        float next_cascade = shadow_cascade_splits[i];
        
        float xn = curr_cascade * tan_half_h_fov;
        float xf = next_cascade * tan_half_h_fov;
        float yn = curr_cascade * tan_half_v_fov;
        float yf = next_cascade * tan_half_v_fov;

        glm::vec4 frustum_corners[] = {
            glm::vec4(+xn, +yn, -curr_cascade, 1.0f),
            glm::vec4(-xn, +yn, -curr_cascade, 1.0f),
            glm::vec4(+xn, -yn, -curr_cascade, 1.0f),
            glm::vec4(-xn, -yn, -curr_cascade, 1.0f),

            glm::vec4(+xf, +yf, -next_cascade, 1.0f),
            glm::vec4(-xf, +yf, -next_cascade, 1.0f),
            glm::vec4(+xf, -yf, -next_cascade, 1.0f),
            glm::vec4(-xf, -yf, -next_cascade, 1.0f),
        };
        
        glm::vec4 frustum_corners_l[8];

        glm::vec3 center_world = glm::vec3(0, 0, 0);

        for (int j = 0; j < 8; j++) {
            glm::vec4 vw = inv_view_matrix * frustum_corners[j];
            center_world += glm::vec3(vw.x, vw.y, vw.z);
        }
        center_world = center_world / 8.0f;
        
        glm::vec3 light_dir = glm::normalize_or_zero(directional_light->direction);
        glm::vec3 light_eye = center_world - light_dir * 1000.0f;

        glm::vec3 world_up = (fabsf(light_dir.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        //world_up = v3(0, 1, 0);
        glm::mat4 light_view = glm::lookAt(light_eye, center_world, world_up);
        
        float min_x = FLT_MAX, max_x = -FLT_MAX;
        float min_y = FLT_MAX, max_y = -FLT_MAX;
        float min_z = FLT_MAX, max_z = -FLT_MAX;
        
        for (int j = 0; j < 8; j++) {
            glm::vec4 vw = inv_view_matrix * frustum_corners[j];

            frustum_corners_l[j] = light_view * vw;

            min_x = Min(min_x, frustum_corners_l[j].x);
            min_y = Min(min_y, frustum_corners_l[j].y);
            min_z = Min(min_z, frustum_corners_l[j].z);
            
            max_x = Max(max_x, frustum_corners_l[j].x);
            max_y = Max(max_y, frustum_corners_l[j].y);
            max_z = Max(max_z, frustum_corners_l[j].z);
        }

        float world_units_per_texel = (max_x - min_x) / (float)SHADOW_MAP_WIDTH;
        min_x = floorf(min_x / world_units_per_texel) * world_units_per_texel;
        max_x = floorf(max_x / world_units_per_texel) * world_units_per_texel;
        min_y = floorf(min_y / world_units_per_texel) * world_units_per_texel;
        max_y = floorf(max_y / world_units_per_texel) * world_units_per_texel;

        float z_pad = 500.0f;
        max_z += z_pad;
        min_z -= z_pad;
        
        glm::mat4 light_proj = glm::ortho(min_x, max_x, max_y, min_y, -max_z, -min_z);
        
        uniforms->light_matrix[i] = light_proj * light_view;

        uniforms->cascade_splits[i] = glm::vec4(shadow_cascade_splits[i], 0.0f, 0.0f, 0.0f);
    }
}

// TODO: Move this out of here eventually. It's here because it needs to be between vkCmdBeginRendering and vkCmdEndRendering
void Scene_Renderer::draw_imgui_stuff() {
    MyZoneScopedN("ImGui Rendering");
    static float current_dt = 1.0f;
    static int frame_counter = 0;

    frame_counter++;
    if (frame_counter > 30) {
        current_dt = (float)globals.time_info.delta_time_seconds;
        frame_counter = 0;
    }
    
    ImGui::Begin("Stats");
    ImGui::Text("FPS: %d", (int)(1.0f / current_dt));
    ImGui::Text("Frame time: %.2fms", current_dt * 1000.0f);
    ImGui::Text("GPU: %s", backend->selected_gpu_name);

    double total_available_vram_mb, used_vram_mb;
    backend->get_memory_usage_info(&total_available_vram_mb, &used_vram_mb);
    ImGui::Text("VRAM: %d/%d MB", (int)used_vram_mb, (int)total_available_vram_mb);
    
    ImGui::End();
}

static glm::vec3 hsv_to_rgb(const glm::vec3& hsv) {
    float h = hsv.x * 6.0f;
    float s = hsv.y;
    float v = hsv.z;

    int i = (int)floor(h) % 6;
    float f = h - floor(h);
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    switch (i) {
        case 0: return glm::vec3(v, t, p);
        case 1: return glm::vec3(q, v, p);
        case 2: return glm::vec3(p, v, t);
        case 3: return glm::vec3(p, q, v);
        case 4: return glm::vec3(t, p, v);
        case 5: return glm::vec3(v, p, q);
    }
    return glm::vec3(1, 1, 1);
}

void Scene_Renderer::draw_hud(VkExtent2D extent) {
    renderer_2d->draw_quad(globals.white_texture, {50, 50}, {64, 64}, FLIP_MODE_NONE, NULL, {1, 0.5f, 0.2f, 1});

    Texture *circle_texture = globals.texture_registry->find_or_load("circle");
    float radius = 0.01f * extent.height;
    renderer_2d->draw_quad(circle_texture, {extent.width * 0.5f - radius, extent.height * 0.5f - radius}, {radius*2, radius*2}, FLIP_MODE_NONE, NULL, {1, 1, 1, 1});

    int pad = (int)(0.0025f * extent.width);
    
    int font_size = (int)(0.04f * extent.height);
    Dynamic_Font *font = get_font_at_size("KarminaBoldItalic", font_size);
    char *text = "Slavchi e gei!";
    int x = extent.width  - font->get_string_width_in_pixels(text) - pad;
    int y = extent.height - font->character_height;

    float t = (float)nanoseconds_to_seconds(globals.time_info.real_world_time);
    glm::vec3 rgb = hsv_to_rgb(glm::vec3(fmodf(t * 0.3f, 1.0f), 1.0f, 1.0f));
    glm::vec4 color = glm::vec4(rgb, 1.0f);

    glm::vec4 shadow_color = glm::vec4(0, 0, 0, 1);
    int offset = font->character_height / 20;

    if (offset) {
        renderer_2d->draw_text(font, text, x-offset, y, glm::vec4(0,0,0,1));
        renderer_2d->draw_text(font, text, x+offset, y, glm::vec4(0,0,0,1));
        renderer_2d->draw_text(font, text, x, y-offset, glm::vec4(0,0,0,1));
        renderer_2d->draw_text(font, text, x, y+offset, glm::vec4(0,0,0,1));
        renderer_2d->draw_text(font, text, x-offset, y-offset, glm::vec4(0,0,0,1));
        renderer_2d->draw_text(font, text, x+offset, y-offset, glm::vec4(0,0,0,1));
        renderer_2d->draw_text(font, text, x-offset, y+offset, glm::vec4(0,0,0,1));
        renderer_2d->draw_text(font, text, x+offset, y+offset, glm::vec4(0,0,0,1));
    }
    
    renderer_2d->draw_text(font, text, x, y, color);
}

void Scene_Renderer::draw_mesh(VkCommandBuffer cb, Mesh *mesh, glm::mat4 const &world_matrix, glm::vec4 scale_color, int cascade_index) {
    Per_Object_Uniforms per_object_uniforms = {};

    per_object_uniforms.world_matrix         = world_matrix;
    per_object_uniforms.scale_color          = scale_color;
    per_object_uniforms.shadow_cascade_index = cascade_index;

    Assert(pipeline_layout_for_current_pass == mesh_pipeline_layout);
    vkCmdPushConstants(cb, pipeline_layout_for_current_pass, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Per_Object_Uniforms), &per_object_uniforms);

    for (int i = 0; i < mesh->num_submeshes; i++) {
        Submesh *submesh = &mesh->submeshes[i];
        if (!submesh->has_gpu_data) {
            generate_gpu_data_for_submesh(submesh);
            if (!submesh->has_gpu_data) continue;
        }

        MyZoneScopedN("Draw one submesh");
        
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_for_current_pass, 1, 1, &submesh->material.descriptor_set, 0, NULL);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &submesh->vertex_buffer.buffer, &offset);
        vkCmdBindIndexBuffer(cb, submesh->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cb, submesh->num_indices, 1, 0, 0, 0);
    }
}

void Scene_Renderer::draw_mesh_instanced(VkCommandBuffer cb, Mesh *mesh, Gpu_Buffer *instance_buffer, int offset, int count) {
    for (int i = 0; i < mesh->num_submeshes; i++) {
        Submesh *submesh = &mesh->submeshes[i];
        if (!submesh->has_gpu_data) {
            generate_gpu_data_for_submesh(submesh);
            if (!submesh->has_gpu_data) continue;
        }

        MyZoneScopedN("Draw one submesh instanced");

        Assert(pipeline_layout_for_current_pass == mesh_instanced_pipeline_layout);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_for_current_pass, 1, 1, &submesh->material.descriptor_set, 0, NULL);

        VkBuffer vertex_buffers[] = { submesh->vertex_buffer.buffer, instance_buffer->buffer };
        //VkDeviceSize offsets[] = { 0, (VkDeviceSize)offset };
        VkDeviceSize offsets[] = { 0, 0 };
        vkCmdBindVertexBuffers(cb, 0, 2, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(cb, submesh->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cb, submesh->num_indices, count, 0, 0, offset);
    }
}

void Scene_Renderer::draw_fullscreen_quad(VkCommandBuffer cb, VkPipeline pipeline, VkDescriptorSet descriptor_set) {
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreen_quad_pipeline_layout, 0, 1, &per_scene_uniforms_descriptor_sets[backend->current_frame], 0, NULL);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreen_quad_pipeline_layout, 1, 1, &descriptor_set, 0, NULL);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &fullscreen_quad_vertex_buffer.buffer, &offset);
    vkCmdBindIndexBuffer(cb, fullscreen_quad_index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
}

bool Scene_Renderer::create_per_scene_vulkan_objects() {
    VkDescriptorPoolSize per_scene_uniforms_pool_sizes[2] = {};

    per_scene_uniforms_pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    per_scene_uniforms_pool_sizes[0].descriptorCount = Render_Backend::MAX_FRAMES_IN_FLIGHT;

    per_scene_uniforms_pool_sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    per_scene_uniforms_pool_sizes[1].descriptorCount = (MAX_SHADOW_CASCADES + 1) * Render_Backend::MAX_FRAMES_IN_FLIGHT;
    
    if (!backend->create_descriptor_pool(&per_scene_uniforms_descriptor_pool, ArrayCount(per_scene_uniforms_pool_sizes), per_scene_uniforms_pool_sizes, Render_Backend::MAX_FRAMES_IN_FLIGHT)) {
        return false;
    }

    eastl::vector <VkDescriptorSetLayoutBinding> per_scene_uniforms_descriptor_set_layout_bindings;
    per_scene_uniforms_descriptor_set_layout_bindings.resize(2 + MAX_SHADOW_CASCADES);
    
    per_scene_uniforms_descriptor_set_layout_bindings[0].binding         = 0;
    per_scene_uniforms_descriptor_set_layout_bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    per_scene_uniforms_descriptor_set_layout_bindings[0].descriptorCount = 1;
    per_scene_uniforms_descriptor_set_layout_bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    for (int i = 1; i < MAX_SHADOW_CASCADES + 2; i++) {
        per_scene_uniforms_descriptor_set_layout_bindings[i].binding         = i;
        per_scene_uniforms_descriptor_set_layout_bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        per_scene_uniforms_descriptor_set_layout_bindings[i].descriptorCount = 1;
        per_scene_uniforms_descriptor_set_layout_bindings[i].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    
    if (!backend->create_descriptor_set_layout(&per_scene_uniforms_descriptor_set_layout, (int)per_scene_uniforms_descriptor_set_layout_bindings.size(), per_scene_uniforms_descriptor_set_layout_bindings.data())) {
        return false;
    }
    
    if (!backend->create_descriptor_sets(per_scene_uniforms_descriptor_pool, per_scene_uniforms_descriptor_set_layout, Render_Backend::MAX_FRAMES_IN_FLIGHT, per_scene_uniforms_descriptor_sets)) {
        return false;
    }

    for (int i = 0; i < Render_Backend::MAX_FRAMES_IN_FLIGHT; i++) {
        if (!backend->create_buffer(&per_scene_uniform_buffers[i], VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT , sizeof(Per_Scene_Uniforms), NULL)) {
            return false;
        }

        backend->update_descriptor_set(per_scene_uniforms_descriptor_sets[i], 0, &per_scene_uniform_buffers[i]);
    }

    return true;
}

bool Scene_Renderer::create_material_vulkan_objects() {
    int max_submeshes_per_render_entity = 8;
    
    VkDescriptorPoolSize material_uniforms_pool_sizes[2] = {};

    material_uniforms_pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    material_uniforms_pool_sizes[0].descriptorCount = MAX_RENDER_ENTITIES * max_submeshes_per_render_entity;

    material_uniforms_pool_sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    material_uniforms_pool_sizes[1].descriptorCount = MAX_RENDER_ENTITIES * max_submeshes_per_render_entity * 5; // TODO: Remove this magic number make it a texture type enum for example

    if (!backend->create_descriptor_pool(&material_uniforms_descriptor_pool, ArrayCount(material_uniforms_pool_sizes), material_uniforms_pool_sizes, MAX_RENDER_ENTITIES * max_submeshes_per_render_entity)) {
        return false;
    }

    VkDescriptorSetLayoutBinding material_uniforms_descriptor_set_layout_bindings[6] = {};

    material_uniforms_descriptor_set_layout_bindings[0].binding = 0;
    material_uniforms_descriptor_set_layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    material_uniforms_descriptor_set_layout_bindings[0].descriptorCount = 1;
    material_uniforms_descriptor_set_layout_bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    for (int i = 1; i < 6; i++) {
        material_uniforms_descriptor_set_layout_bindings[i].binding = i;
        material_uniforms_descriptor_set_layout_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        material_uniforms_descriptor_set_layout_bindings[i].descriptorCount = 1;
        material_uniforms_descriptor_set_layout_bindings[i].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    if (!backend->create_descriptor_set_layout(&material_uniforms_descriptor_set_layout, ArrayCount(material_uniforms_descriptor_set_layout_bindings), material_uniforms_descriptor_set_layout_bindings)) {
        return false;
    }
    
    return true;
}

bool Scene_Renderer::create_fullscreen_quad_vulkan_objects() {
    VkDescriptorPoolSize fullscreen_quad_pool_sizes[2] = {};
    
    fullscreen_quad_pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    fullscreen_quad_pool_sizes[0].descriptorCount = MAX_RESOLVE_PASSES * Render_Backend::MAX_FRAMES_IN_FLIGHT;

    fullscreen_quad_pool_sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    fullscreen_quad_pool_sizes[1].descriptorCount = MAX_RESOLVE_PASSES * 4 * Render_Backend::MAX_FRAMES_IN_FLIGHT;

    if (!backend->create_descriptor_pool(&fullscreen_quad_descriptor_pool, ArrayCount(fullscreen_quad_pool_sizes), fullscreen_quad_pool_sizes, MAX_RESOLVE_PASSES * 4 * Render_Backend::MAX_FRAMES_IN_FLIGHT)) {
        return false;
    }

    VkDescriptorSetLayoutBinding fullscreen_quad_descriptor_set_layout_bindings[5] = {};

    fullscreen_quad_descriptor_set_layout_bindings[0].binding = 0;
    fullscreen_quad_descriptor_set_layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    fullscreen_quad_descriptor_set_layout_bindings[0].descriptorCount = 1;
    fullscreen_quad_descriptor_set_layout_bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    for (int i = 1; i < 5; i++) {
        fullscreen_quad_descriptor_set_layout_bindings[i].binding = i;
        fullscreen_quad_descriptor_set_layout_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        fullscreen_quad_descriptor_set_layout_bindings[i].descriptorCount = 1;
        fullscreen_quad_descriptor_set_layout_bindings[i].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    if (!backend->create_descriptor_set_layout(&fullscreen_quad_descriptor_set_layout, ArrayCount(fullscreen_quad_descriptor_set_layout_bindings), fullscreen_quad_descriptor_set_layout_bindings)) {
        return false;
    }

    if (!backend->create_descriptor_sets(fullscreen_quad_descriptor_pool, fullscreen_quad_descriptor_set_layout, MAX_RESOLVE_PASSES * Render_Backend::MAX_FRAMES_IN_FLIGHT, fullscreen_quad_descriptor_sets)) return false;

    for (int i = 0; i < Render_Backend::MAX_FRAMES_IN_FLIGHT; i++) {
        if (!backend->create_buffer(&ssao_kernel_uniform_buffers[i], VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, SSAO_KERNEL_SIZE * sizeof(glm::vec4), NULL)) {
            return false;
        }

        for (int j = 0; j < MAX_RESOLVE_PASSES; j++) {
            backend->update_descriptor_set(fullscreen_quad_descriptor_sets[i * Render_Backend::MAX_FRAMES_IN_FLIGHT + j], 0, &ssao_kernel_uniform_buffers[i]);
        }
    }
    
    Immediate_Vertex fullscreen_quad_vertices[] = {
        { { -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } },
        { { +1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f } },
        { { +1.0f, +1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },
        { { -1.0f, +1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },
    };

    u32 fullscreen_quad_indices[] = {
        0, 1, 2,
        0, 2, 3,
    };

    if (!backend->create_buffer(&fullscreen_quad_vertex_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(fullscreen_quad_vertices), fullscreen_quad_vertices)) return false;
    if (!backend->create_buffer(&fullscreen_quad_index_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof(fullscreen_quad_indices), fullscreen_quad_indices)) return false;

    return true;
}

bool Scene_Renderer::create_pipeline_layouts() {
    VkPushConstantRange per_object_push_constant_range = {};
    per_object_push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    per_object_push_constant_range.offset = 0;
    per_object_push_constant_range.size = sizeof(Per_Object_Uniforms);
    
    VkDescriptorSetLayout mesh_descriptor_set_layouts[] = {
        per_scene_uniforms_descriptor_set_layout,
        material_uniforms_descriptor_set_layout,
    };
    
    if (!backend->create_graphics_pipeline_layout(ArrayCount(mesh_descriptor_set_layouts), mesh_descriptor_set_layouts, 1, &per_object_push_constant_range, &mesh_pipeline_layout)) return false;
    if (!backend->create_graphics_pipeline_layout(ArrayCount(mesh_descriptor_set_layouts), mesh_descriptor_set_layouts, 0, NULL, &mesh_instanced_pipeline_layout)) return false;

    VkDescriptorSetLayout fullscreen_quad_descriptor_set_layouts[] = {
        per_scene_uniforms_descriptor_set_layout,
        fullscreen_quad_descriptor_set_layout,
    };
    if (!backend->create_graphics_pipeline_layout(ArrayCount(fullscreen_quad_descriptor_set_layouts), fullscreen_quad_descriptor_set_layouts, 0, NULL, &fullscreen_quad_pipeline_layout)) return false;
    
    return true;
}

bool Scene_Renderer::create_pipelines() {
    Graphics_Pipeline_Info mesh_pipeline_info = {};
    mesh_pipeline_info.pipeline_layout = mesh_pipeline_layout;
    mesh_pipeline_info.shader_filename = "basic";
    mesh_pipeline_info.vertex_type     = RENDER_VERTEX_MESH;
    mesh_pipeline_info.blend_mode      = BLEND_MODE_OFF;
    mesh_pipeline_info.cull_mode       = CULL_MODE_BACK;
    mesh_pipeline_info.depth_test_mode = DEPTH_TEST_MODE_LEQUAL;
    mesh_pipeline_info.depth_write     = true;
    mesh_pipeline_info.color_write     = true;
    VkFormat main_pass_color_attachment_formats[2] = {
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
    };
    mesh_pipeline_info.num_color_attachment_formats = 1;
    mesh_pipeline_info.color_attachment_formats = &main_pass_color_attachment_formats[0];
    mesh_pipeline_info.depth_attachment_format = VK_FORMAT_D32_SFLOAT;//depth_buffer.format;
    if (!backend->create_graphics_pipeline(mesh_pipeline_info, &mesh_pipeline)) {
        return false;
    }

    mesh_pipeline_info.num_color_attachment_formats = ArrayCount(main_pass_color_attachment_formats);
    mesh_pipeline_info.color_attachment_formats = main_pass_color_attachment_formats;
    mesh_pipeline_info.shader_filename = "z_prepass";
    if (!backend->create_graphics_pipeline(mesh_pipeline_info, &z_prepass_pipeline)) {
        return false;
    }

    mesh_pipeline_info.pipeline_layout = mesh_instanced_pipeline_layout;
    mesh_pipeline_info.shader_filename = "basic_instanced";
    mesh_pipeline_info.vertex_type     = RENDER_VERTEX_MESH_INSTANCED;
    mesh_pipeline_info.num_color_attachment_formats = 1;
    mesh_pipeline_info.color_attachment_formats = &main_pass_color_attachment_formats[0];
    if (!backend->create_graphics_pipeline(mesh_pipeline_info, &mesh_instanced_pipeline)) {
        return false;
    }
    
    Graphics_Pipeline_Info shadow_pipeline_info = {};
    shadow_pipeline_info.pipeline_layout = mesh_pipeline_layout;
    shadow_pipeline_info.shader_filename = "shadow";
    shadow_pipeline_info.vertex_type     = RENDER_VERTEX_MESH;
    shadow_pipeline_info.blend_mode      = BLEND_MODE_OFF;
    shadow_pipeline_info.cull_mode       = CULL_MODE_NONE;
    shadow_pipeline_info.depth_test_mode = DEPTH_TEST_MODE_LEQUAL;
    shadow_pipeline_info.depth_write     = true;
    shadow_pipeline_info.color_write     = false;
    shadow_pipeline_info.depth_attachment_format = VK_FORMAT_D32_SFLOAT;//shadow_map_buffers[0].format;
    if (!backend->create_graphics_pipeline(shadow_pipeline_info, &shadow_pipeline)) {
        return false;
    }

    shadow_pipeline_info.pipeline_layout = mesh_instanced_pipeline_layout;
    shadow_pipeline_info.shader_filename = "shadow_instanced";
    shadow_pipeline_info.vertex_type     = RENDER_VERTEX_MESH_INSTANCED;
    if (!backend->create_graphics_pipeline(shadow_pipeline_info, &shadow_instanced_pipeline)) {
        return false;
    }

    Graphics_Pipeline_Info resolve_pipeline_info = {};
    resolve_pipeline_info.pipeline_layout = fullscreen_quad_pipeline_layout;
    resolve_pipeline_info.shader_filename = "resolve";
    resolve_pipeline_info.vertex_type     = RENDER_VERTEX_IMMEDIATE;
    resolve_pipeline_info.blend_mode      = BLEND_MODE_ALPHA;
    resolve_pipeline_info.cull_mode       = CULL_MODE_NONE;
    resolve_pipeline_info.depth_test_mode = DEPTH_TEST_MODE_OFF;
    resolve_pipeline_info.depth_write     = false;
    resolve_pipeline_info.color_write     = true;
    resolve_pipeline_info.num_color_attachment_formats = 1;
    VkFormat resolve_pass_format = backend->get_swap_chain_surface_format();
    resolve_pipeline_info.color_attachment_formats = &resolve_pass_format;
    if (!backend->create_graphics_pipeline(resolve_pipeline_info, &resolve_pipeline)) {
        return false;
    }

    VkFormat ssao_format = VK_FORMAT_R16_SFLOAT;
    resolve_pipeline_info.shader_filename          = "ssao";
    resolve_pipeline_info.color_attachment_formats = &ssao_format;
    resolve_pipeline_info.blend_mode               = BLEND_MODE_OFF;
    if (!backend->create_graphics_pipeline(resolve_pipeline_info, &ssao_pipeline)) {
        return false;
    }

    resolve_pipeline_info.shader_filename = "ssao_blur";
    if (!backend->create_graphics_pipeline(resolve_pipeline_info, &ssao_blur_pipeline)) {
        return false;
    }

    return true;
}
