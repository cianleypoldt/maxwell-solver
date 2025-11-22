#ifndef C_UTILS
#define C_UTILS
#include <stdint.h>

char* load_txt_file(const char* path);

typedef enum { VERTEX, FRAGMENT } shader_type;

uint32_t compile_shader_from_path(const char* path, shader_type type);
#endif
