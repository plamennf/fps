#pragma once

const int MAX_FONTS = 128;

struct Texture;

struct Glyph {
    int size_x, size_y;
    int bearing_x, bearing_y;
    Rectangle2i src_rect;
    u32 advance;
};

struct Font {
    char *filepath;
    int character_height;

    int default_line_spacing;
    int max_ascender;
    int max_descender;
    int typical_ascender;
    int typical_descender;
    int em_width;
    int x_advance;

    int y_offset_for_centering;
    
    Glyph glyphs[128]; // Only ASCII support for now. @TODO: Add unicode support.
    Texture *texture;
};

bool load_font(Font *font, char *filepath, int character_height);
void destroy_fonts();
int get_text_width(Font *font, char *text);
Font *get_font_at_size(char *filepath, int font_size);
