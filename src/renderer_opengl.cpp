#include "main.h"
#include "renderer_opengl.h"
#include "mesh.h"

#include <stddef.h>
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static SDL_Window *window;

static Framebuffer_Gl framebuffers[MAX_FRAMEBUFFERS];
static int num_framebuffers;

static Gpu_Buffer_Gl gpu_buffers[MAX_GPU_BUFFERS];
static int num_gpu_buffers;

static Shader_Gl shaders[MAX_SHADERS];
static int num_shaders;

static Texture_Gl textures[MAX_TEXTURES];
static int num_textures;

static Shader *current_shader;

static Framebuffer_Gl *add_framebuffer() {
    assert(num_framebuffers < MAX_FRAMEBUFFERS);
    return &framebuffers[num_framebuffers++];
}

static Gpu_Buffer_Gl *add_gpu_buffer() {
    assert(num_gpu_buffers < MAX_GPU_BUFFERS);
    return &gpu_buffers[num_gpu_buffers++];
}

static Shader_Gl *add_shader() {
    assert(num_shaders < MAX_SHADERS);
    return &shaders[num_shaders++];
}

static Texture_Gl *add_texture() {
    assert(num_textures < MAX_TEXTURES);
    return &textures[num_textures++];
}

void init_renderer(SDL_Window *_window) {
    window = _window;

    void immediate_init();
    immediate_init();

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glEnable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_MULTISAMPLE);

    //globals.transform.wvp_matrix = matrix4_identity();
    globals.transform.projection_matrix = matrix4_identity();
    globals.transform.view_matrix = matrix4_identity();
    globals.transform.world_matrix = matrix4_identity();
}

void swap_buffers() {
    SDL_GL_SwapWindow(window);
}

void set_blend_mode(Blend_Mode blend_mode) {
    switch (blend_mode) {
        case BLEND_MODE_OFF: {
            glDisable(GL_BLEND);
        } break;

        case BLEND_MODE_ALPHA: {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } break;
    }
}

void set_depth_test(Depth_Test_Mode depth_test_mode) {
    switch (depth_test_mode) {
        case DEPTH_TEST_OFF: {
            glDisable(GL_DEPTH_TEST);
        } break;

        case DEPTH_TEST_LEQUAL: {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
        } break;

        case DEPTH_TEST_LESS: {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
        } break;
    }
}

void set_cull_face(Cull_Face cull_face) {
    glFrontFace(GL_CCW);
    
    switch (cull_face) {
        case CULL_FACE_NONE: {
            glDisable(GL_CULL_FACE);
        } break;

        case CULL_FACE_BACK: {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        } break;

        case CULL_FACE_FRONT: {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
        } break;
    }
}

void set_depth_write(bool write) {
    glDepthMask(write ? GL_TRUE : GL_FALSE);
}

void draw_indexed(u32 num_indices, u32 first_index) {
    u64 offset = (u64)first_index * sizeof(u32);
    glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, (void *)offset);
}

void draw_non_indexed(u32 num_vertices, u32 first_vertex) {
    glDrawArrays(GL_TRIANGLES, first_vertex, num_vertices);
}

static int get_bpp(Texture_Format format) {
    switch (format) {
        case TEXTURE_FORMAT_RGBA8: return 4;
        case TEXTURE_FORMAT_RGB8:  return 3;
        case TEXTURE_FORMAT_RG8:   return 2;
        case TEXTURE_FORMAT_R8 :   return 1;
    }

    assert(!"Invalid color texture format");
    return 0;
}

Texture *make_texture(int width, int height, Texture_Format format, u8 *data, char *filepath) {
    Texture_Gl *texture = add_texture();

    texture->width    = width;
    texture->height   = height;
    texture->format   = format;
    texture->bpp      = get_bpp(format);
    texture->filepath = copy_string(filepath);

    switch (texture->bpp) {
        case 4: {
            texture->internal_format = GL_SRGB8_ALPHA8;
            texture->source_format   = GL_RGBA;
        } break;

        case 3: {
            texture->internal_format = GL_SRGB8;
            texture->source_format   = GL_RGB;
        } break;

        case 2: {
            texture->internal_format = GL_RG8;
            texture->source_format   = GL_RG;
        } break;

        case 1: {
            texture->internal_format = GL_R8;
            texture->source_format   = GL_RED;
        } break;
    }

    glGenTextures(1, &texture->id);
    glBindTexture(GL_TEXTURE_2D, texture->id);
    glTexImage2D(GL_TEXTURE_2D, 0, texture->internal_format, width, height, 0, texture->source_format, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return texture;
}

Texture *load_texture(char *filepath) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(1);
    stbi_uc *data = stbi_load(filepath, &width, &height, &channels, 0);
    if (!data) {
        logprintf("Failed to read file '%s'\n", filepath);
        return NULL;
    }
    defer { stbi_image_free(data); };

    Texture_Format format = TEXTURE_FORMAT_UNKNOWN;
    switch (channels) {
        case 4: {
            format = TEXTURE_FORMAT_RGBA8;
        } break;

        case 3: {
            format = TEXTURE_FORMAT_RGB8;
        } break;

        case 2: {
            format = TEXTURE_FORMAT_RG8;
        } break;

        case 1: {
            format = TEXTURE_FORMAT_R8;
        } break;
    }
    
    return make_texture(width, height, format, data, filepath);
}

Texture *load_cubemap(char *filepaths_of_faces[6]) {
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id);

    int width, height, channels;
    stbi_set_flip_vertically_on_load(0);
    for (int i = 0; i < 6; i++) {
        stbi_uc *data = stbi_load(filepaths_of_faces[i], &width, &height, &channels, 4);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        if (data) {
            stbi_image_free(data);
            data = NULL;
        } else {
            logprintf("Failed to load '%s' cubemap texture!\n", filepaths_of_faces[i]);
            return NULL;
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    Texture_Gl *texture = add_texture();

    texture->width    = width;
    texture->height   = height;
    //texture->format   = format;
    //texture->bpp      = get_bpp(format);
    //texture->filepath = copy_string(filepath);
    texture->id       = texture_id;
    
    return (Texture *)texture;
}

void destroy_texture(Texture *_texture) {
    if (!_texture) return;
    
    Texture_Gl *texture = (Texture_Gl *)_texture;
    texture->width  = 0;
    texture->height = 0;
    texture->bpp    = 0;
    texture->internal_format = 0;
    texture->source_format   = 0;

    if (texture->id) {
        glDeleteTextures(1, &texture->id);
        texture->id = 0;
    }
}

void set_texture(Texture_Type type, Texture *_texture) {
    assert(_texture);
    Texture_Gl *texture = (Texture_Gl *)_texture;
    
    glActiveTexture(GL_TEXTURE0 + (int)type);
    glBindTexture(GL_TEXTURE_2D, texture->id);
}

void set_color_texture(Framebuffer *_framebuffer) {
    assert(_framebuffer);
    Framebuffer_Gl *framebuffer = (Framebuffer_Gl *)_framebuffer;

    glActiveTexture(GL_TEXTURE0);
    if (globals.antialiasing_type == ANTIALIASING_MSAA_8X ||
        globals.antialiasing_type == ANTIALIASING_MSAA_4X ||
        globals.antialiasing_type == ANTIALIASING_MSAA_2X) {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, framebuffer->color_id);
    } else {
        glBindTexture(GL_TEXTURE_2D, framebuffer->color_id);
    }
}

void set_shadow_map(Framebuffer *_framebuffer, int index) {
    assert(_framebuffer);
    Framebuffer_Gl *framebuffer = (Framebuffer_Gl *)_framebuffer;

    glActiveTexture(GL_TEXTURE3 + index);
    glBindTexture(GL_TEXTURE_2D, framebuffer->depth_id);
}

void set_cube_map(Texture *_texture) {
    assert(_texture);
    Texture_Gl *texture = (Texture_Gl *)_texture;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture->id);
}

Framebuffer *make_framebuffer(int width, int height, Texture_Format color_format, Texture_Format depth_format) {
    assert(color_format != TEXTURE_FORMAT_UNKNOWN || depth_format != TEXTURE_FORMAT_UNKNOWN);
    assert(color_format == TEXTURE_FORMAT_RGBA8 || color_format == TEXTURE_FORMAT_UNKNOWN);
    assert(depth_format == TEXTURE_FORMAT_D24S8 || depth_format == TEXTURE_FORMAT_SHADOW_MAP || depth_format == TEXTURE_FORMAT_UNKNOWN);

    Framebuffer_Gl *framebuffer = add_framebuffer();

    framebuffer->width  = width;
    framebuffer->height = height;

    framebuffer->color_format = color_format;
    framebuffer->depth_format = depth_format;
    
    glGenFramebuffers(1, &framebuffer->fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->fbo_id);

    if (color_format != TEXTURE_FORMAT_UNKNOWN) {
        glGenTextures(1, &framebuffer->color_id);
    }

    if (depth_format != TEXTURE_FORMAT_UNKNOWN) {
        if (depth_format == TEXTURE_FORMAT_SHADOW_MAP) {
            glGenTextures(1, &framebuffer->depth_id);
        } else {
            glGenRenderbuffers(1, &framebuffer->depth_id);
        }
    }

    /*
    if (globals.multisampling) {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, framebuffer->color_id);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, globals.num_multisamples, GL_SRGB8_ALPHA8, width, height, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, framebuffer->color_id, 0);

        glBindRenderbuffer(GL_RENDERBUFFER, framebuffer->depth_id);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, globals.num_multisamples, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, framebuffer->depth_id);
        } else {
    */

    if (color_format != TEXTURE_FORMAT_UNKNOWN) {
        glBindTexture(GL_TEXTURE_2D, framebuffer->color_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer->color_id, 0);
    }

    if (depth_format != TEXTURE_FORMAT_UNKNOWN) {
        if (depth_format == TEXTURE_FORMAT_SHADOW_MAP) {
            glBindTexture(GL_TEXTURE_2D, framebuffer->depth_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); 
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            float border_color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer->depth_id, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        } else {
            glBindRenderbuffer(GL_RENDERBUFFER, framebuffer->depth_id);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, framebuffer->depth_id);
        }
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        logprintf("Framebuffer(%dx%d) is not complete!!!\n", width, height);
        assert(!"Incomplete framebuffer");
    }

    return (Framebuffer *)framebuffer;
}

Framebuffer *make_multisampled_framebuffer(int width, int height, Texture_Format color_format, Texture_Format depth_format, int num_multisamples) {
    assert(color_format != TEXTURE_FORMAT_UNKNOWN || depth_format != TEXTURE_FORMAT_UNKNOWN);
    assert(color_format == TEXTURE_FORMAT_RGBA8 || color_format == TEXTURE_FORMAT_UNKNOWN);
    assert(depth_format == TEXTURE_FORMAT_D24S8 || depth_format == TEXTURE_FORMAT_UNKNOWN);

    Framebuffer_Gl *framebuffer = add_framebuffer();

    framebuffer->width  = width;
    framebuffer->height = height;

    framebuffer->color_format = color_format;
    framebuffer->depth_format = depth_format;
    
    glGenFramebuffers(1, &framebuffer->fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->fbo_id);

    if (color_format != TEXTURE_FORMAT_UNKNOWN) {
        glGenTextures(1, &framebuffer->color_id);
    }

    if (depth_format != TEXTURE_FORMAT_UNKNOWN) {
        if (depth_format == TEXTURE_FORMAT_SHADOW_MAP) {
            glGenTextures(1, &framebuffer->depth_id);
        } else {
            glGenRenderbuffers(1, &framebuffer->depth_id);
        }
    }

    if (color_format != TEXTURE_FORMAT_UNKNOWN) {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, framebuffer->color_id);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, num_multisamples, GL_SRGB8_ALPHA8, width, height, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, framebuffer->color_id, 0);
    }

    if (depth_format != TEXTURE_FORMAT_UNKNOWN) {
        glBindRenderbuffer(GL_RENDERBUFFER, framebuffer->depth_id);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, num_multisamples, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, framebuffer->depth_id);
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        logprintf("Framebuffer(%dx%d) is not complete!!!\n", width, height);
        assert(!"Incomplete framebuffer");
    }

    return (Framebuffer *)framebuffer;
}

void destroy_framebuffer(Framebuffer *_framebuffer) {
    if (!_framebuffer) return;
    Framebuffer_Gl *framebuffer = (Framebuffer_Gl *)_framebuffer;

    if (framebuffer->color_id) {
        glDeleteTextures(1, &framebuffer->color_id);
        framebuffer->color_id = 0;
    }

    if (framebuffer->depth_id) {
        glDeleteRenderbuffers(1, &framebuffer->depth_id);
        framebuffer->depth_id = 0;
    }

    if (framebuffer->fbo_id) {
        glDeleteFramebuffers(1, &framebuffer->fbo_id);
        framebuffer->fbo_id = 0;
    }
}

void set_framebuffer(Framebuffer *_framebuffer, bool clear_color, Vector4 color, bool clear_depth, float z, bool clear_stencil, u8 stencil) {
    if (_framebuffer == NULL) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        globals.render_target_width  = globals.window_width;
        globals.render_target_height = globals.window_height;
    } else {
        auto framebuffer = (Framebuffer_Gl *)_framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->fbo_id);
        globals.render_target_width  = framebuffer->width;
        globals.render_target_height = framebuffer->height;
    }

    glViewport(0, 0, globals.render_target_width, globals.render_target_height);

    GLenum clear_flags = 0;
    if (clear_color) {
        glClearColor(color.r, color.g, color.b, color.a);
        clear_flags |= GL_COLOR_BUFFER_BIT;
    }

    if (clear_depth) {
        glClearDepth(z);
        clear_flags |= GL_DEPTH_BUFFER_BIT;
    }

    if (clear_stencil) {
        glClearStencil(stencil);
        clear_flags |= GL_STENCIL_BUFFER_BIT;
    }

    glClear(clear_flags);
}

Gpu_Buffer *make_gpu_buffer(Gpu_Buffer_Type type, u32 size, void *data, bool is_dynamic) {
    Gpu_Buffer_Gl *buffer = add_gpu_buffer();
    buffer->type = type;
    buffer->size = size;
    buffer->is_dynamic = is_dynamic;

    GLenum gl_type = 0;
    switch (type) {
        case GPU_BUFFER_VERTEX: {
            gl_type = GL_ARRAY_BUFFER;
        } break;

        case GPU_BUFFER_INDEX: {
            gl_type = GL_ELEMENT_ARRAY_BUFFER;
        } break;

        default: {
            assert(!"Invalid buffer type");
        } break;
    }
    
    glGenBuffers(1, &buffer->id);
    glBindBuffer(gl_type, buffer->id);
    glBufferData(gl_type, size, data, is_dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    glBindBuffer(gl_type, 0);
    
    return (Gpu_Buffer *)buffer;
}

static void set_vertex_format(Render_Vertex_Type vertex_type) {
    switch (vertex_type) {
        case RENDER_VERTEX_MESH: {
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Mesh_Vertex), (void *)offsetof(Mesh_Vertex, position));
            glEnableVertexAttribArray(0);

            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Mesh_Vertex), (void *)offsetof(Mesh_Vertex, uv));
            glEnableVertexAttribArray(1);
            
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Mesh_Vertex), (void *)offsetof(Mesh_Vertex, normal));
            glEnableVertexAttribArray(2);

            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Mesh_Vertex), (void *)offsetof(Mesh_Vertex, tangent));
            glEnableVertexAttribArray(3);

            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Mesh_Vertex), (void *)offsetof(Mesh_Vertex, bitangent));
            glEnableVertexAttribArray(4);
        } break;

        case RENDER_VERTEX_IMMEDIATE: {
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), (void *)offsetof(Immediate_Vertex, position));
            glEnableVertexAttribArray(0);

            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), (void *)offsetof(Immediate_Vertex, color));
            glEnableVertexAttribArray(1);

            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), (void *)offsetof(Immediate_Vertex, uv));
            glEnableVertexAttribArray(2);

            glDisableVertexAttribArray(3);
            glDisableVertexAttribArray(4);
        } break;
    }
}

void set_vertex_buffer(Gpu_Buffer *_vertex_buffer) {
    assert(_vertex_buffer);
    Gpu_Buffer_Gl *vertex_buffer = (Gpu_Buffer_Gl *)_vertex_buffer;
    
    assert(vertex_buffer->type == GPU_BUFFER_VERTEX);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer->id);

    Shader *_shader = current_shader;
    assert(_shader);

    Shader_Gl *shader = (Shader_Gl *)_shader;
    set_vertex_format(shader->vertex_type);
}

void set_index_buffer(Gpu_Buffer *_index_buffer) {
    assert(_index_buffer);
    Gpu_Buffer_Gl *index_buffer = (Gpu_Buffer_Gl *)_index_buffer;
    
    assert(index_buffer->type == GPU_BUFFER_INDEX);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer->id);
}

void update_current_gpu_buffer(Gpu_Buffer *_buffer, u32 offset, u32 size, void *data) {
    assert(_buffer);
    Gpu_Buffer_Gl *buffer = (Gpu_Buffer_Gl *)_buffer;

    GLenum gl_type = 0;
    switch (buffer->type) {
        case GPU_BUFFER_VERTEX: {
            gl_type = GL_ARRAY_BUFFER;
        } break;

        case GPU_BUFFER_INDEX: {
            gl_type = GL_ELEMENT_ARRAY_BUFFER;
        } break;

        default: {
            assert(!"Invalid buffer type");
        } break;
    }
    
    glBufferSubData(gl_type, offset, size, data);
}

Shader *load_shader(char *filepath, Render_Vertex_Type vertex_type) {
    char *file_data = read_entire_file(filepath);
    if (!file_data) {
        logprintf("Failed to read file '%s'\n", filepath);
        return NULL;
    }
    defer { delete [] file_data; };

    char *vertex_source[] = {
        "#version 420 core\n#define VERTEX_SHADER\n#define OUT_IN out\n#line 1 1\n",
        file_data
    };
    
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    defer { glDeleteShader(v); };
    glShaderSource(v, ArrayCount(vertex_source), vertex_source, NULL);
    glCompileShader(v);
    int success;
    glGetShaderiv(v, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[4096];
        glGetShaderInfoLog(v, sizeof(info_log), NULL, info_log);
        logprintf("Failed to compile '%s' vertex shader:\n%s\n", filepath, info_log);
        return NULL;
    }

    GLuint g = 0;
    if (strstr(file_data, "GEOMETRY_SHADER")) {
        char *geometry_source[] = {
            "#version 420 core\n#define GEOMETRY_SHADER\n#line 1 1\n",
            file_data
        };
        
        g = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(g, ArrayCount(geometry_source), geometry_source, NULL);
        glCompileShader(g);
        glGetShaderiv(g, GL_COMPILE_STATUS, &success);
        if (!success) {
            char info_log[4096];
            glGetShaderInfoLog(g, sizeof(info_log), NULL, info_log);
            logprintf("Failed to compile '%s' geometry shader:\n%s\n", filepath, info_log);
            return NULL;
        }
    }
    defer { if (g > 0) { glDeleteShader(g); } };

    char *fragment_source[] = {
        "#version 420 core\n#define FRAGMENT_SHADER\n#define OUT_IN in\n#line 1 1\n",
        file_data
    };
    
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    defer { glDeleteShader(f); };
    glShaderSource(f, ArrayCount(fragment_source), fragment_source, NULL);
    glCompileShader(f);
    glGetShaderiv(f, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[4096];
        glGetShaderInfoLog(f, sizeof(info_log), NULL, info_log);
        logprintf("Failed to compile '%s' fragment shader:\n%s\n", filepath, info_log);
        return NULL;
    }

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    if (g > 0) {
        glAttachShader(p, g);
    }
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[4096];
        glGetProgramInfoLog(p, sizeof(info_log), NULL, info_log);
        logprintf("Failed to link '%s' shader program:\n%s\n", filepath, info_log);
        glDeleteProgram(p);
        return NULL;
    }
    glValidateProgram(p);
    glGetProgramiv(p, GL_VALIDATE_STATUS, &success);
    if (!success) {
        char info_log[4096];
        glGetProgramInfoLog(p, sizeof(info_log), NULL, info_log);
        logprintf("Failed to validate '%s' shader program:\n%s\n", filepath, info_log);
        glDeleteProgram(p);
        return NULL;
    }

    Shader_Gl *shader = add_shader();

    shader->vertex_type = vertex_type;
    
    shader->program_id = p;

#define GUL(loc) shader->##loc = glGetUniformLocation(p, #loc)
    GUL(diffuse_texture);
    GUL(specular_texture);
    GUL(normal_texture);
    GUL(projection_matrix);
    GUL(view_matrix);
    GUL(world_matrix);
    GUL(num_multisamples);
    GUL(camera_position);
    GUL(light_matrix);
#undef GUL

    shader->material_color     = glGetUniformLocation(p, "material.color");
    shader->material_shininess = glGetUniformLocation(p, "material.shininess");
    shader->material_use_normal_map = glGetUniformLocation(p, "material.use_normal_map");
    
    shader->directional_light_direction = glGetUniformLocation(p, "directional_light.direction");
    shader->directional_light_ambient = glGetUniformLocation(p, "directional_light.ambient");
    shader->directional_light_diffuse = glGetUniformLocation(p, "directional_light.diffuse");
    shader->directional_light_specular = glGetUniformLocation(p, "directional_light.specular");

    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        char name[128];
        snprintf(name, sizeof(name), "point_lights[%d].position", i);
        shader->point_light_position[i] = glGetUniformLocation(p, name);

        snprintf(name, sizeof(name), "point_lights[%d].constant", i);
        shader->point_light_constant[i] = glGetUniformLocation(p, name);

        snprintf(name, sizeof(name), "point_lights[%d].linear", i);
        shader->point_light_linear[i] = glGetUniformLocation(p, name);

        snprintf(name, sizeof(name), "point_lights[%d].quadratic", i);
        shader->point_light_quadratic[i] = glGetUniformLocation(p, name);

        snprintf(name, sizeof(name), "point_lights[%d].ambient", i);
        shader->point_light_ambient[i] = glGetUniformLocation(p, name);

        snprintf(name, sizeof(name), "point_lights[%d].diffuse", i);
        shader->point_light_diffuse[i] = glGetUniformLocation(p, name);

        snprintf(name, sizeof(name), "point_lights[%d].specular", i);
        shader->point_light_specular[i] = glGetUniformLocation(p, name);
    }

    for (int i = 0; i < NUM_SHADOW_MAP_CASCADES; i++) {
        char name[128];
        snprintf(name, sizeof(name), "shadow_map_textures[%d]", i);
        shader->shadow_map_textures[i] = glGetUniformLocation(p, name);

        snprintf(name, sizeof(name), "light_matrices[%d]", i);
        shader->light_matrices[i] = glGetUniformLocation(p, name);

        snprintf(name, sizeof(name), "cascade_splits[%d]", i);
        shader->cascade_splits[i] = glGetUniformLocation(p, name);
    }

    shader->spot_light_position = glGetUniformLocation(p, "spot_light.position");
    shader->spot_light_direction = glGetUniformLocation(p, "spot_light.direction");
    shader->spot_light_cut_off = glGetUniformLocation(p, "spot_light.cut_off");
    shader->spot_light_outer_cut_off = glGetUniformLocation(p, "spot_light.outer_cut_off");
    shader->spot_light_ambient = glGetUniformLocation(p, "spot_light.ambient");
    shader->spot_light_diffuse = glGetUniformLocation(p, "spot_light.diffuse");
    shader->spot_light_specular = glGetUniformLocation(p, "spot_light.specular");
    shader->spot_light_constant = glGetUniformLocation(p, "spot_light.constant");
    shader->spot_light_linear = glGetUniformLocation(p, "spot_light.linear");
    shader->spot_light_quadratic = glGetUniformLocation(p, "spot_light.quadratic");
    
    glUseProgram(p);
    glUniform1i(shader->diffuse_texture, 0);
    glUniform1i(shader->specular_texture, 1);
    glUniform1i(shader->normal_texture, 2);
    glUniform1i(shader->shadow_map_textures[0], 3);
    glUniform1i(shader->shadow_map_textures[1], 4);
    glUniform1i(shader->shadow_map_textures[2], 5);
    glUniform1i(shader->shadow_map_textures[3], 6);
    glUseProgram(0);
    
    return (Shader *)shader;
}

void set_shader(Shader *_shader) {
    if (current_shader == _shader) return;

    current_shader = _shader;
    
    if (!_shader) {
        glUseProgram(0);
        return;
    }

    Shader_Gl *shader = (Shader_Gl *)_shader;

    glUseProgram(shader->program_id);

    refresh_transform();
    refresh_lights();
    refresh_csm();

    if (shader->num_multisamples != -1) {
        int get_num_multisamples(Antialiasing_Type type);
        glUniform1i(shader->num_multisamples, get_num_multisamples(globals.antialiasing_type));
    }
}

Shader *get_current_shader() {
    return current_shader;
}

static void set_int(GLint loc, int v) {
    if (loc == -1) return;
    glUniform1i(loc, v);
}

static void set_float(GLint loc, float v) {
    if (loc == -1) return;
    glUniform1f(loc, v);
}

static void set_vector2(GLint loc, Vector2 v) {
    if (loc == -1) return;
    glUniform2f(loc, v.x, v.y);
}

static void set_vector3(GLint loc, Vector3 v) {
    if (loc == -1) return;
    glUniform3f(loc, v.x, v.y, v.z);
}

static void set_vector4(GLint loc, Vector4 v) {
    if (loc == -1) return;
    glUniform4f(loc, v.x, v.y, v.z, v.w);
}

static void set_matrix4(GLint loc, Matrix4 m) {
    if (loc == -1) return;
    glUniformMatrix4fv(loc, 1, GL_TRUE, &m._11);
}

void refresh_transform() {
    if (current_shader) {
        Shader_Gl *shader = (Shader_Gl *)current_shader;
        
        set_matrix4(shader->projection_matrix, globals.transform.projection_matrix);
        set_matrix4(shader->view_matrix, globals.transform.view_matrix);
        set_matrix4(shader->world_matrix, globals.transform.world_matrix);
        set_matrix4(shader->light_matrix, globals.transform.light_matrix);
    }
}

void refresh_material(Material *material) {
    if (!current_shader) return;
    
    Shader_Gl *shader = (Shader_Gl *)current_shader;
    
    set_vector4(shader->material_color, material->diffuse_color);
    set_float(shader->material_shininess, material->shininess);
    set_int(shader->material_use_normal_map, material->normal_texture ? 1 : 0);
}

void refresh_lights() {
    if (!current_shader) return;

    Shader_Gl *shader = (Shader_Gl *)current_shader;

    set_vector3(shader->camera_position, globals.camera.position);
    
    set_vector3(shader->directional_light_direction, globals.directional_light.direction);
    set_vector3(shader->directional_light_ambient, globals.directional_light.ambient);
    set_vector3(shader->directional_light_diffuse, globals.directional_light.diffuse);
    set_vector3(shader->directional_light_specular, globals.directional_light.specular);

    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        set_vector3(shader->point_light_position[i], globals.point_lights[i].position);
        set_float(shader->point_light_constant[i], globals.point_lights[i].constant);
        set_float(shader->point_light_linear[i], globals.point_lights[i].linear);
        set_float(shader->point_light_quadratic[i], globals.point_lights[i].quadratic);
        set_vector3(shader->point_light_ambient[i], globals.point_lights[i].ambient);
        set_vector3(shader->point_light_diffuse[i], globals.point_lights[i].diffuse);
        set_vector3(shader->point_light_specular[i], globals.point_lights[i].specular);
    }

    set_vector3(shader->spot_light_position, globals.spot_light.position);
    set_vector3(shader->spot_light_direction, globals.spot_light.direction);
    set_float(shader->spot_light_cut_off, globals.spot_light.cut_off);
    set_float(shader->spot_light_outer_cut_off, globals.spot_light.outer_cut_off);
    set_vector3(shader->spot_light_ambient, globals.spot_light.ambient);
    set_vector3(shader->spot_light_diffuse, globals.spot_light.diffuse);
    set_vector3(shader->spot_light_specular, globals.spot_light.specular);
    set_float(shader->spot_light_constant, globals.spot_light.constant);
    set_float(shader->spot_light_linear, globals.spot_light.linear);
    set_float(shader->spot_light_quadratic, globals.spot_light.quadratic);
}

void refresh_csm() {
    if (!current_shader) return;

    Shader_Gl *shader = (Shader_Gl *)current_shader;

    for (int i = 0; i < NUM_SHADOW_MAP_CASCADES; i++) {
        set_matrix4(shader->light_matrices[i], globals.shadow_map_cascade_matrices[i]);
        set_float(shader->cascade_splits[i], globals.shadow_map_cascade_splits[i]);
    }
}
