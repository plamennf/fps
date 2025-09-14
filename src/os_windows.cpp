#ifdef _WIN32

#include "general.h"

#include <Windows.h>

static void to_windows_filepath(char *filepath, wchar_t *wide_filepath, int wide_filepath_size) {
    MultiByteToWideChar(CP_UTF8, 0, filepath, -1, wide_filepath, wide_filepath_size);

    for (wchar_t *at = wide_filepath; *at; at++) {
        if (*at == L'/') {
            *at = L'\\';
        }
    }
}

static void to_normal_filepath(wchar_t *wide_filepath, char *filepath, int filepath_size) {
    WideCharToMultiByte(CP_UTF8, 0, wide_filepath, -1, filepath, filepath_size, NULL, 0);

    for (char *at = filepath; *at; at++) {
        if (*at == '\\') {
            *at = '/';
        }
    }
}

bool os_file_exists(char *filepath) {
    wchar_t wide_filepath[4096];
    to_windows_filepath(filepath, wide_filepath, ArrayCount(wide_filepath));
    return GetFileAttributesW(wide_filepath) != INVALID_FILE_ATTRIBUTES;
}

bool os_get_file_last_write_time(char *filepath, u64 *modtime_pointer) {
    wchar_t wide_filepath[4096];
    to_windows_filepath(filepath, wide_filepath, ArrayCount(wide_filepath));

    HANDLE file = CreateFileW(wide_filepath, 0,//GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    defer { CloseHandle(file); };

    FILETIME ft_create, ft_access, ft_write;
    if (!GetFileTime(file, &ft_create, &ft_access, &ft_write)) return false;
    
    ULARGE_INTEGER uli;
    uli.LowPart  = ft_write.dwLowDateTime;
    uli.HighPart = ft_write.dwHighDateTime;
    
    if (modtime_pointer) *modtime_pointer = uli.QuadPart;
    
    return true;
}

#endif
