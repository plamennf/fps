#pragma once

#include <stdint.h>
#include <assert.h>
#include <string.h>

#ifdef _MSC_VER
#define COMPILER_MSVC
#endif

#ifdef COMPILER_MSVC
#define HAS_INTRINSIC_SUPPORT
#include <intrin.h>
#define DoDebugBreak() __debugbreak()
#endif

#include <new>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t  s64;
typedef int32_t  s32;
typedef int16_t  s16;
typedef int8_t   s8;

// Copy-paste from https://gist.github.com/andrewrk/ffb272748448174e6cdb4958dae9f3d8
// Defer macro/thing.

#define CONCAT_INTERNAL(x,y) x##y
#define CONCAT(x,y) CONCAT_INTERNAL(x,y)

template<typename T>
struct ExitScope {
    T lambda;
    ExitScope(T lambda):lambda(lambda){}
    ~ExitScope(){lambda();}
    ExitScope(const ExitScope&);
  private:
    ExitScope& operator =(const ExitScope&);
};
 
class ExitScopeHelp {
  public:
    template<typename T>
        ExitScope<T> operator+(T t){ return t;}
};
 
#define defer const auto& CONCAT(defer__, __LINE__) = ExitScopeHelp() + [&]()

#define ArrayCount(arr) (sizeof(arr)/sizeof((arr)[0]))
#define Max(x, y) ((x) > (y) ? (x) : (y))
#define Min(x, y) ((x) < (y) ? (x) : (y))

#define Bit(x) (1 << (x))
#define Square(x) ((x)*(x))

#if defined(BUILD_DEBUG) || defined(BUILD_RELEASE)
#define ASSERTIONS_ENABLED
#endif

#ifdef ASSERTIONS_ENABLED
#define Assert(expr) if (expr) {} else { DoDebugBreak(); }
#else
#define Assert(expr)
#endif

// From https://www.bytesbeneath.com/p/the-arena-custom-memory-allocators
#define IsPowerOfTwo(x) ((x != 0) && ((x & (x - 1)) == 0))

#define Kilobytes(x) ((x)*1024ULL)
#define Megabytes(x) (Kilobytes(x)*1024ULL)
#define Gigabytes(x) (Megabytes(x)*1024ULL)
#define Terabytes(x) (Gigabytes(x)*1024ULL)

// Overload for eastl
inline void* __cdecl operator new[](size_t size, const char* name, int flags, unsigned debugFlags, const char* file, int line) {
	return new uint8_t[size];
}

inline void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line) {
    return new uint8_t[size];
}

struct System_Time {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

u64 round_to_next_power_of_2(u64 v);

s64 string_length(char *s);
char *copy_string(char *s);
bool strings_match(char *a, char *b);
bool strings_match(const char *a, const char *b);
bool strings_match(char *a, s64 a_len, char *b);

int get_codepoint(char *text, int *bytes_processed);

char *eat_spaces(char *s);
char *eat_trailing_spaces(char *s);

char *consume_next_line(char **text_ptr);
bool starts_with(char *a, char *b);

char *find_character_from_left(char *s, char c);

bool is_end_of_line(char c);
bool is_space(char c);

void clamp(float *value, float min, float max);
void clamp(int *value, int min, int max);
void clamp(u32 *value, u32 min, u32 max);

void init_log();
void close_log();
void logprintf(char *fmt, ...);

char *read_entire_file(char *filepath, s64 *length_pointer = NULL, bool zero_terminate = true);
bool file_exists(char *filepath);
void move_file(char *old_filepath, char *new_filepath);

char *break_by_space(char *s);
char *break_by_comma(char *s);

float fract(float value);
float random_float();
float random_float(float min, float max);

s64 get_time_nanoseconds();

System_Time get_system_time();
System_Time get_local_time();
char *get_name_of_user();
