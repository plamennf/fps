#include "main.h"
#include "console.h"
#include "draw.h"

enum Console_State {
    CONSOLE_STATE_CLOSED,
    CONSOLE_STATE_OPEN,
};

static Console_State current_state = CONSOLE_STATE_CLOSED;
static float openness_t        = 0.0f;
static float openness_t_max    = 0.4f;
static float openness_t_target = 0.0f;

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
}

bool is_console_open() {
    return openness_t > 0.0f;
}
