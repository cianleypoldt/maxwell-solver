#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstdint>

typedef struct {
        void* buffer;
        long  size;
} file;

file load_file(const char* path) {
        FILE* fp = fopen(path, "rb");
        if (!fp) {
                return (file) { NULL, 0 };
        }

        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (size < 0) {
                fclose(fp);
                return (file) { NULL, 0 };
        }
        void* buffer = malloc(size);
        if (!buffer) {
                fclose(fp);
                return (file) { NULL, 0 };
        }

        fread(buffer, 1, size, fp);
        fclose(fp);

        return (file) { buffer, size };
}

void free_file(file f) {
        free(f.buffer);
}
