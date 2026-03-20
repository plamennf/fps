#include "pch.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef _WIN32
#include <Windows.h>
#include <Lmcons.h>

static char name_of_user_buffer[UNLEN + 1];
static LARGE_INTEGER global_perf_freq;

#else

static Uint64 global_perf_freq;

#endif

static FILE *log_file = NULL;

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

bool strings_match(const char *a, const char *b) {
    return strings_match((char *)a, (char *)b);
}

bool strings_match(char *a, s64 a_len, char *b) {
    if (a == b) return true;
    if (!a || !b) return false;

    for (s64 i = 0; i < a_len; i++) {
        if (b[i] == 0) return false;
        if (a[i] != b[i]) return false;
    }

    return b[a_len] == 0;
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

void clamp(u32 *value, u32 min, u32 max) {
    if (!value) return;

    if (*value < min) *value = min;
    if (*value > max) *value = max;    
}

void init_log() {
    if (!log_file) {
        log_file = fopen("log.txt", "wt");
        if (!log_file) {
            fprintf(stderr, "Failed to open log.txt for writing\n");
            fflush(stderr);
        }
    }
}

void close_log() {
    if (log_file) {
        fflush(log_file);
        fclose(log_file);
        log_file = NULL;
    }
}

void logprintf(char *fmt, ...) {
    char buf[4096];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

#if defined(BUILD_DEBUG) || defined(__EMSCRIPTEN__)
    fprintf(stdout, "%s", buf);
    fflush(stdout);
#endif

    if (log_file) {
        fprintf(log_file, "%s", buf);
        fflush(log_file);
    }
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

bool file_exists(char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) return false;
    fclose(file);
    return true;
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

float random_float() {
    return (float)rand() / (float)RAND_MAX;
}


#ifdef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

#ifdef __cplusplus
}
#endif

// Call this before creating your SDL window or any GUI.
void enable_dpi_awareness() {
    // Try Windows 10+ per-monitor V2 awareness
    HMODULE shcore = LoadLibraryA("Shcore.dll");
    if (shcore) {
        typedef HRESULT(WINAPI *SetProcessDpiAwarenessFn)(int);
        SetProcessDpiAwarenessFn SetProcessDpiAwarenessPtr =
            (SetProcessDpiAwarenessFn)GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (SetProcessDpiAwarenessPtr) {
            // 2 = PROCESS_PER_MONITOR_DPI_AWARE
            SetProcessDpiAwarenessPtr(2);
            FreeLibrary(shcore);
            return;
        }
        FreeLibrary(shcore);
    }

    // Try Windows 8.1+ API (SetProcessDpiAwarenessContext)
    HMODULE user32 = LoadLibraryA("User32.dll");
    if (user32) {
        typedef BOOL(WINAPI *SetProcessDpiAwarenessContextFn)(HANDLE);
        SetProcessDpiAwarenessContextFn SetProcessDpiAwarenessContextPtr =
            (SetProcessDpiAwarenessContextFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (SetProcessDpiAwarenessContextPtr) {
            // -4 = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
            SetProcessDpiAwarenessContextPtr((HANDLE)(-4));
            FreeLibrary(user32);
            return;
        }
        FreeLibrary(user32);
    }

    // Fallback for Windows 7 and older
    HMODULE user32_old = LoadLibraryA("User32.dll");
    if (user32_old) {
        typedef BOOL(WINAPI *SetProcessDPIAwareFn)(void);
        SetProcessDPIAwareFn SetProcessDPIAwarePtr =
            (SetProcessDPIAwareFn)GetProcAddress(user32_old, "SetProcessDPIAware");
        if (SetProcessDPIAwarePtr) {
            SetProcessDPIAwarePtr();
        }
        FreeLibrary(user32_old);
    }
}

void move_file(char *old_filepath, char *new_filepath) {
    wchar_t wide_old_filepath[1024];
    MultiByteToWideChar(CP_UTF8, 0, old_filepath, -1, wide_old_filepath, ArrayCount(wide_old_filepath));

    for (wchar_t *at = wide_old_filepath; *at; at++) {
        if (*at == L'/') {
            *at = L'\\';
        }
    }

    wchar_t wide_new_filepath[1024];
    MultiByteToWideChar(CP_UTF8, 0, new_filepath, -1, wide_new_filepath, ArrayCount(wide_new_filepath));

    for (wchar_t *at = wide_new_filepath; *at; at++) {
        if (*at == L'/') {
            *at = L'\\';
        }
    }

    MoveFileExW(wide_old_filepath, wide_new_filepath, MOVEFILE_REPLACE_EXISTING);
}

System_Time get_system_time() {
    SYSTEMTIME system_time;
    GetSystemTime(&system_time);

    System_Time result;

    result.year   = system_time.wYear;
    result.month  = system_time.wMonth;
    result.day    = system_time.wDay;
    result.hour   = system_time.wHour;
    result.minute = system_time.wMinute;
    result.second = system_time.wSecond;

    return result;
}

System_Time get_local_time() {
    SYSTEMTIME system_time;
    GetLocalTime(&system_time);

    System_Time result;

    result.year   = system_time.wYear;
    result.month  = system_time.wMonth;
    result.day    = system_time.wDay;
    result.hour   = system_time.wHour;
    result.minute = system_time.wMinute;
    result.second = system_time.wSecond;

    return result;
}

char *get_name_of_user() {
    wchar_t buffer[UNLEN + 1];
    DWORD size = sizeof(buffer) / sizeof(wchar_t);

    GetUserNameW(buffer, &size);

    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, name_of_user_buffer, sizeof(name_of_user_buffer), NULL, NULL);

    return name_of_user_buffer;
}

s64 get_time_nanoseconds() {
    if (!global_perf_freq.QuadPart) {
        QueryPerformanceFrequency(&global_perf_freq);
    }

    LARGE_INTEGER perf_counter;
    QueryPerformanceCounter(&perf_counter);

    return (s64)((perf_counter.QuadPart * 1000000000ULL) / (global_perf_freq.QuadPart ? global_perf_freq.QuadPart : 1));
}

#else

s64 get_time_nanoseconds() {
    if (!global_perf_freq) {
        global_perf_freq = SDL_GetPerformanceFrequency();
    }
    
    Uint64 counter = SDL_GetPerformanceCounter();
    return (Uint64)((counter * 1000000000ULL) / (global_perf_freq ? global_perf_freq : 1));
}

#endif
