#include <ft2build.h>
#include FT_FREETYPE_H

#include "main.h"
#include "font.h"

static FT_Library ft_library;
static bool ft_library_initted;

static Font loaded_fonts[MAX_FONTS];
static int num_loaded_fonts;

inline int FT_ROUND(int x) {
    if (x >= 0) return (x + 0x1f) >> 6;
    return -(((-x) + 0x1f) >> 6);
}

bool load_font(Font *font, char *filepath, int character_height) {
    if (!ft_library_initted) {
        if (FT_Init_FreeType(&ft_library)) {
            logprintf("Failed to initialize FreeType!\n");
            return false;
        }
        ft_library_initted = true;
    }
    
    font->character_height = character_height;

    FT_Face face;
    if (FT_New_Face(ft_library, filepath, 0, &face)) {
        logprintf("Failed to load font '%s'.\n", filepath);
        return false;
    }
    defer { FT_Done_Face(face); };

    FT_Set_Pixel_Sizes(face, 0, character_height);
    
    u64 total_prepass_area = 0;
    for (u8 c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            logprintf("Failed to load glyph for '%c'\n", c);
            continue;
        }

        total_prepass_area += (u64)(face->glyph->bitmap.width + 8) * character_height;
    }

    int texture_width  = (int)round_to_next_power_of_2((u64)ceil(sqrt((double)total_prepass_area)));
    int texture_height = texture_width;
    u8 *texture_data   = new u8[(s64)texture_width * (s64)texture_height * 4];
    memset(texture_data, 0, (s64)texture_width * (s64)texture_height * 4);
    defer { delete [] texture_data; };
    int texture_cursor_x = 0;
    int texture_cursor_y = 0;

    for (u8 c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            // We don't print anything since we printed in the prepass for loop above.
            continue;
        }

        Glyph *glyph     = &font->glyphs[c];
        glyph->size_x    = face->glyph->bitmap.width;
        glyph->size_y    = face->glyph->bitmap.rows;
        glyph->bearing_x = face->glyph->bitmap_left;
        glyph->bearing_y = face->glyph->bitmap_top;
        glyph->advance   = face->glyph->advance.x >> 6;

        if (c <= 32 || c >= 127) continue;

        bool is_already_on_a_new_line = false;
        if (texture_cursor_x + glyph->size_x > texture_width) {
            texture_cursor_y += character_height;
            texture_cursor_x = 0;
            is_already_on_a_new_line = true;
        }

        for (int y = 0; y < glyph->size_y; y++) {
            for (int x = 0; x < glyph->size_x; x++) {
                u8 *dest = &texture_data[((y + texture_cursor_y) * texture_width + (x + texture_cursor_x)) * 4];
                u8 source = face->glyph->bitmap.buffer[y * glyph->size_x + x];

                dest[0] = 255;
                dest[1] = 255;
                dest[2] = 255;
                dest[3] = source;
            }
        }

        glyph->src_rect = {
            texture_cursor_x,
            texture_cursor_y,
            glyph->size_x,
            glyph->size_y,
        };

        texture_cursor_x += glyph->size_x + 8;
        if (!is_already_on_a_new_line && texture_cursor_x >= texture_width) {
            texture_cursor_y += character_height;
            texture_cursor_x = 0;
        }
    }

    float y_scale_font_to_pixels = face->size->metrics.y_scale / (64.0f * 65536.0f);
    font->default_line_spacing = (int)floor(y_scale_font_to_pixels * face->height + 0.5f);
    font->max_ascender  = (int)floor(y_scale_font_to_pixels * face->bbox.yMax + 0.5f);
    font->max_descender = (int)floor(y_scale_font_to_pixels * face->bbox.yMin + 0.5f);

    auto glyph_index = FT_Get_Char_Index(face, 'm');
    if (glyph_index) {
        FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        font->y_offset_for_centering = (int)(0.5f * FT_ROUND(face->glyph->metrics.horiBearingY) + 0.5f);
    }

    glyph_index = FT_Get_Char_Index(face, 'M');
    if (glyph_index) {
        FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        font->em_width  = FT_ROUND(face->glyph->metrics.width);
        font->x_advance = FT_ROUND(face->glyph->metrics.horiAdvance);
    }

    glyph_index = FT_Get_Char_Index(face, 'T');
    if (glyph_index) {
        FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        font->typical_ascender = FT_ROUND(face->glyph->metrics.horiBearingY);
    }

    glyph_index = FT_Get_Char_Index(face, 'g');
    if (glyph_index) {
        FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        font->typical_descender = FT_ROUND(face->glyph->metrics.horiBearingY - face->glyph->metrics.height);
    }

    auto error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    assert(!error);

    font->texture = make_texture(texture_width, texture_height, TEXTURE_FORMAT_RGBA8, texture_data);

    font->filepath = copy_string(filepath);
    
    return true;
}

void destroy_font(Font *font) {
    destroy_texture(font->texture);
    if (font->filepath) {
        delete [] font->filepath;
        font->filepath = NULL;
    }
}

void destroy_fonts() {
    for (int i = 0; i < num_loaded_fonts; i++) {
        destroy_font(&loaded_fonts[i]);
    }
    num_loaded_fonts = 0;
}

int get_text_width(Font *font, char *text) {
    int width = 0;

    for (char *at = text; *at; at++) {
        Glyph *glyph = &font->glyphs[*at];
        if (!glyph) continue;

        if (*at == '\n') {
            logprintf("Reached new line in get_text_width: Stopping measuring!\n");
            break;
        }

        width += glyph->advance;
    }
    
    return width;
}

Font *get_font_at_size(char *filepath, int font_size) {
    for (int i = 0; i < num_loaded_fonts; i++) {
        Font *font = &loaded_fonts[i];
        if (strings_match(font->filepath, filepath) && font->character_height == font_size) {
            return font;
        }
    }

    assert(num_loaded_fonts < MAX_FONTS);

    Font font = {};
    if (!load_font(&font, filepath, font_size)) {
        return NULL;
    }

    loaded_fonts[num_loaded_fonts++] = font;
    return &loaded_fonts[num_loaded_fonts - 1];
}
