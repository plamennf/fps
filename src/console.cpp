#include "main.h"
#include "console.h"
#include "draw.h"
#include "font.h"

// TODO:
// - Add scrolling to the console history
// - Add text input and draw it
// - Add commands

enum Console_State {
    CONSOLE_STATE_CLOSED,
    CONSOLE_STATE_OPEN,
};

static Console_State current_state = CONSOLE_STATE_CLOSED;
static float openness_t        = 0.0f;
static float openness_t_max    = 0.4f;
static float openness_t_target = 0.0f;

static Array <char *> console_history;

static void add_string_to_history(char *_s) {
    char *s = copy_string(_s);
    // s = sanitize(s);
    console_history.add(s);
}

void toggle_console() {
    if (current_state == CONSOLE_STATE_OPEN) {
        current_state = CONSOLE_STATE_CLOSED;
        openness_t_target = 0.0f;
        globals.should_show_cursor = false;
    } else {
        current_state = CONSOLE_STATE_OPEN;
        openness_t_target = openness_t_max;
        globals.should_show_cursor = true;
    }
}

void draw_console(float dt) {
    if (console_history.count <= 0) {
        add_string_to_history("noclip");
        add_string_to_history("godmode");
        add_string_to_history("crosshair_size 3");
    }
    
    float console_change_speed = 1.0f;
    openness_t = move_toward(openness_t, openness_t_target, console_change_speed * dt);

    if (openness_t <= 0.0f) return;
    
    //
    // Draw console
    //

    float text_input_part_t = 0.05f;
    float text_input_part   = text_input_part_t * globals.window_height;
    float history_part      = (openness_t - text_input_part_t) * globals.window_height;

    set_shader(globals.shader_color);
    rendering_2d();
    
    if (history_part > 0.0f) {
        immediate_begin();
        immediate_quad(v2(0, globals.window_height - history_part), v2((float)globals.window_width, history_part), v4(32/255.0f, 51/255.0f, 84/255.0f, 1.0f));
        immediate_flush();
    }

    immediate_begin();
    immediate_quad(v2(0, globals.window_height - history_part - text_input_part), v2((float)globals.window_width, text_input_part), v4(21/255.0f, 34/255.0f, 56/255.0f, 1.0f));
    immediate_flush();

    int history_font_size = (int)(0.035f * globals.window_height);
    Font *history_font = get_font_at_size("data/fonts/LiberationMono-Regular.ttf", history_font_size);
    float y = globals.window_height - history_part;
    
    for (int i = console_history.count - 1; i >= 0; i--) {
        char *text = console_history[i];
        draw_text(history_font, text, 0, (int)(y + history_font->y_offset_for_centering), v4(1, 1, 1, 1));
        y += history_font->character_height;
    }
}

bool is_console_open() {
    return openness_t > 0.0f;
}
