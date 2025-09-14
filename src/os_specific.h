#pragma once

bool os_file_exists(char *filepath);
bool os_get_file_last_write_time(char *filepath, u64 *modtime);
