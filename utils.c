#include "utils.h"

#include "external/glad/glad.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

char* load_txt_file(const char* path) {
        FILE* fp = fopen(path, "r");
        if (!fp) {
                return NULL;
        }

        fseek(fp, 0, SEEK_END);
        size_t len = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        char* str = malloc(len + 1);
        if (!str) {
                printf("Error loading file \"");
                printf(path);
                printf("\": Memory allocation failed\n");
                fclose(fp);
                return NULL;
        }

        fread(str, 1, len, fp);
        str[len] = '\0';

        fclose(fp);
        return str;
}

uint32_t compile_shader_from_path(const char* path, shader_type type) {
        char*       src    = load_txt_file(path);
        const char* srcv[] = { src };
        if (!src) {
                printf("Error loading shader from file \"");
                printf(path);
                printf("\"\n");
                free(src);
                return 0;
        }

        GLuint shader = 0;
        if (type == VERTEX) {
                shader = glCreateShader(GL_VERTEX_SHADER);
        } else if (type == FRAGMENT) {
                shader = glCreateShader(GL_FRAGMENT_SHADER);
        }

        glShaderSource(shader, 1, srcv, NULL);
        free(src);

        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success) {
                return shader;
        }

        int len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        char* log = malloc(len + 1);
        glGetShaderInfoLog(shader, len, NULL, log);
        log[len] = '\n';
        printf("Shader loaded from \"");
        printf(path);
        printf("\" failed to compile:\n    ");
        printf(log);
        free(log);
        return 0;
}
