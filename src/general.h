#pragma once

#include <stdint.h>
#include <assert.h>
#include <string.h>

#ifdef COMPILER_MSVC
#include <intrin.h>
#endif

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

u64 round_to_next_power_of_2(u64 v);

s64 string_length(char *s);
char *copy_string(char *s);
bool strings_match(char *a, char *b);

int get_codepoint(char *text, int *bytes_processed);

char *eat_spaces(char *s);
char *eat_trailing_spaces(char *s);

char *consume_next_line(char **text_ptr);
bool starts_with(char *a, char *b);

char *find_character_from_left(char *s, char c);

bool is_end_of_line(char c);
bool is_space(char c);

u64 get_hash(u64 x);
u64 get_hash(char *str);

void clamp(float *value, float min, float max);
void clamp(int *value, int min, int max);

void logprintf(char *fmt, ...);

char *read_entire_file(char *filepath, s64 *length_pointer = NULL, bool zero_terminate = true);

char *break_by_space(char *s);
char *break_by_comma(char *s);

float fract(float value);
