#include "main.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

u64 round_to_next_power_of_2(u64 v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

s64 string_length(char *s) {
    if (!s) return 0;

    s64 len = 0;
    while (*s++) {
        len++;
    }
    return len;
}

char *copy_string(char *s) {
    if (!s) return NULL;

    s64 len = string_length(s);
    char *result = new char[len + 1];
    memcpy(result, s, len + 1);
    return result;
}

bool strings_match(char *a, char *b) {
    if (a == b) return true;
    if (!a || !b) return false;

    while (*a && *b) {
        if (*a != *b) {
            return false;
        }

        a++;
        b++;
    }

    return *a == 0 && *b == 0;
}

// Copy-paste from https://github.com/raysan5/raylib/blob/master/src/rtext.c
int get_codepoint(char *text, int *bytes_processed) {
    int code = 0x3f;
    int octet = (u8)(text[0]);
    *bytes_processed = 1;

    if (octet <= 0x7f) {
        // Only one octet (ASCII range x00-7F)
        code = text[0];
    } else if ((octet & 0xe0) == 0xc0) {
        // Two octets

        // [0]xC2-DF     [1]UTF8-tail(x80-BF)
        u8 octet1 = text[1];
        if ((octet1 == '\0') || ((octet1 >> 6) != 2)) { *bytes_processed = 2; return code; } // Unexpected sequence

        if ((octet >= 0xc2) && (octet <= 0xdf)) {
            code = ((octet & 0x1f) << 6) | (octet1 & 0x3f);
            *bytes_processed = 2;
        }
    } else if ((octet & 0xf0) == 0xe0) {
        u8 octet1 = text[1];
        u8 octet2 = '\0';

        if ((octet1 == '\0') || ((octet1 >> 6) != 2)) { *bytes_processed = 2; return code; } // Unexpected sequence

        octet2 = text[2];

        if ((octet2 == '\0') || ((octet2 >> 6) != 2)) { *bytes_processed = 3; return code; } // Unexpected sequence

        // [0]xE0    [1]xA0-BF       [2]UTF8-tail(x80-BF)
        // [0]xE1-EC [1]UTF8-tail    [2]UTF8-tail(x80-BF)
        // [0]xED    [1]x80-9F       [2]UTF8-tail(x80-BF)
        // [0]xEE-EF [1]UTF8-tail    [2]UTF8-tail(x80-BF)

        if (((octet == 0xe0) && !((octet1 >= 0xa0) && (octet1 <= 0xbf))) ||
            ((octet == 0xed) && !((octet1 >= 0x80) && (octet1 <= 0x9f)))) {
            *bytes_processed = 2;
            return code;
        }

        if ((octet >= 0xe0) && (0 <= 0xef)) {
            code = ((octet & 0xf) << 12) | ((octet1 & 0x3f) << 6) | (octet2 & 0x3f);
            *bytes_processed = 3;
        }
    } else if ((octet & 0xf8) == 0xf0) {
        // Four octets
        if (octet > 0xf4) return code;

        u8 octet1 = text[1];
        u8 octet2 = '\0';
        u8 octet3 = '\0';

        if ((octet1 == '\0') || ((octet1 >> 6) != 2)) { *bytes_processed = 2; return code; } // Unexpected sequence

        octet2 = text[2];

        if ((octet2 == '\0') || ((octet2 >> 6) != 2)) { *bytes_processed = 3; return code; } // Unexpected sequence

        octet3 = text[3];

        if ((octet3 == '\0') || ((octet3 >> 6) != 2)) { *bytes_processed = 4; return code; }  // Unexpected sequence

        // [0]xF0       [1]x90-BF       [2]UTF8-tail  [3]UTF8-tail
        // [0]xF1-F3    [1]UTF8-tail    [2]UTF8-tail  [3]UTF8-tail
        // [0]xF4       [1]x80-8F       [2]UTF8-tail  [3]UTF8-tail

        if (((octet == 0xf0) && !((octet1 >= 0x90) && (octet1 <= 0xbf))) ||
            ((octet == 0xf4) && !((octet1 >= 0x80) && (octet1 <= 0x8f)))) {
            *bytes_processed = 2; return code; // Unexpected sequence
        }

        if (octet >= 0xf0) {
            code = ((octet & 0x7) << 18) | ((octet1 & 0x3f) << 12) | ((octet2 & 0x3f) << 6) | (octet3 & 0x3f);
            *bytes_processed = 4;
        }
    }

    if (code > 0x10ffff) code = 0x3f; // Codepoints after U+10ffff are invalid

    return code;
}

char *eat_spaces(char *s) {
    if (!s) return NULL;

    while (is_space(*s)) {
        s++;
    }

    return s;
}

char *eat_trailing_spaces(char *s) {
    if (!s) return NULL;
    
    char *end = s + string_length(s) - 1;
    while (end > s && is_space(*end)) end--;

    end[1] = 0;

    return s;
}

char *consume_next_line(char **text_ptr) {
    char *t = *text_ptr;
    if (!*t) return NULL;

    char *s = t;

    while (*t && (*t != '\n') && (*t != '\r')) t++;

    char *end = t;
    if (*t) {
        end++;

        if (*t == '\r') {
            if (*end == '\n') ++end;
        }

        *t = '\0';
    }
    
    *text_ptr = end;
    
    return s;
}

bool starts_with(char *a, char *b) {
    if (a == b) return true;
    if (!a || !b) return false;

    s64 a_len = string_length(a);
    s64 b_len = string_length(b);
    if (a_len < b_len) return false;

    for (s64 i = 0; i < b_len; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }

    return true;
}

char *find_character_from_left(char *s, char c) {
    if (!s) return NULL;

    while (1) {
        if (!*s) return NULL;
        if (*s == c) return s;
        s++;
    }

    return NULL;
}

bool is_end_of_line(char c) {
    bool result = ((c == '\n') ||
                   (c == '\r'));
    return result;
}

bool is_space(char c) {
    bool result = (is_end_of_line(c) ||
                   (c == '\v') ||
                   (c == '\t') ||
                   (c == ' '));
    return result;
}

u64 get_hash(u64 x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

u64 get_hash(char *str) {
    u64 hash = 5381;
    for (char *at = str; *at; at++) {
        hash = ((hash << 5) + hash) + *at;
    }
    return hash;
}

void clamp(float *value, float min, float max) {
    if (!value) return;

    if (*value < min) *value = min;
    if (*value > max) *value = max;
}

void clamp(int *value, int min, int max) {
    if (!value) return;

    if (*value < min) *value = min;
    if (*value > max) *value = max;
}

void logprintf(char *fmt, ...) {
    char buf[4096];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    fprintf(stdout, "%s", buf);
    fflush(stdout);
}

char *read_entire_file(char *filepath, s64 *length_pointer, bool zero_terminate) {
    char *result = NULL;
    if (length_pointer) *length_pointer = 0;
    
    FILE *file = fopen(filepath, "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        auto length = ftell(file);
        fseek(file, 0, SEEK_SET);

        if (zero_terminate) length += 1;
        
        result = new char[length];
        auto num_read = fread(result, 1, zero_terminate ? length - 1 : length, file);
        fclose(file);

        if (length_pointer) *length_pointer = num_read;
        if (zero_terminate) result[num_read] = 0;
    }
    return result;
}

char *break_by_space(char *s) {
    if (!s) return NULL;
    if (*s == 0) return NULL;

    char *start = s;
    char *end   = start;
    while (*end != ' ' && *end != '\n' && *end != '\0') {
        end++;
    }

    if (*end != '\0') {
        *end = 0;
        end++;
    }

    return end;
}

char *break_by_comma(char *s) {
    if (!s) return NULL;
    if (*s == 0) return NULL;

    char *start = s;
    char *end   = start;
    while (*end != ',' && *end != '\n' && *end != '\0') {
        end++;
    }

    if (*end != '\0') {
        *end = 0;
        end++;
    }

    return end;
}

float fract(float value) {
    int intvalue = (int)value;
    float fractpart = value - intvalue;
    return fractpart;
}
