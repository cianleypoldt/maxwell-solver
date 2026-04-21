#include "render.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GLuint create_program(const char * vs_path, const char * fs_path);

float main_quad_vertices[] = {
    -1, -1, 0, 0, 0, 1, -1, 0, 1, 0, 1, 1, 0, 1, 1, -1, -1, 0, 0, 0, 1, 1, 0, 1, 1, -1, 1, 0, 0, 1,
};
GLuint volumetric_prog, vbo, vao, tex_3d;

void init_renderer(simctx * ctx, int width, int height) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow * window_ptr = glfwCreateWindow(width, height, "floating", NULL, NULL);
    glfwSetInputMode(window_ptr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwMakeContextCurrent(window_ptr);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glViewport(0, 0, width, height);

    volumetric_prog = create_program("shaders/vol_vs.glsl", "shaders/vol_fs.glsl");

    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBufferData(GL_ARRAY_BUFFER, sizeof(main_quad_vertices), main_quad_vertices, GL_STATIC_DRAW);

    glGenTextures(1, &tex_3d);

    glBindTexture(GL_TEXTURE_3D, tex_3d);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    int    texture_height, texture_width, texture_depth;
    void * voxel_data;

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, texture_width, texture_height, texture_depth, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 voxel_data);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, tex_3d);
    glUniform1i(glGetUniformLocation(volumetric_prog, "volume"), 0);
}

void deinit_renderer() {
    glDeleteTextures(1, &tex_3d);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(volumetric_prog);
    glfwTerminate();
}

struct file {
    char * buffer;
    int    size;
};

static struct file load_file(const char * path) {
    FILE * fp = fopen(path, "rb");

    if (!fp) {
        return (struct file) { NULL, 0 };
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0) {
        fclose(fp);
        return (struct file) { NULL, 0 };
    }
    void * buffer = malloc(size);
    if (!buffer) {
        fclose(fp);
        return (struct file) { NULL, 0 };
    }

    fread(buffer, 1, size, fp);
    fclose(fp);

    return (struct file) { buffer, size };
}

static void free_file(struct file f) {
    free(f.buffer);
}

static int compile_with_logs(GLuint shader_id) {
    glCompileShader(shader_id);

    int success;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE) {
        return 1;
    }

    int len;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &len);
    char * log = malloc(len + 1);
    glGetShaderInfoLog(shader_id, len + 1, NULL, log);
    log[len] = '\0';
    printf("Shader failed to compile: \n");
    printf("%s\n", log);
    free(log);
    return 0;
}

static int link_shader_with_logs(GLuint prog, GLuint vs, GLuint fs) {
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (linked) {
        return 1;
    }

    GLint len = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    char * log = malloc(len ? len : 1);
    glGetProgramInfoLog(prog, len, NULL, log);
    printf("Failed to link shaders: \n");
    printf("%s\n", log);
    free(log);

    return 0;
}

GLuint create_program(const char * vs_path, const char * fs_path) {
    GLuint vertex_shader   = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    {
        struct file vs = load_file(vs_path);
        struct file fs = load_file(fs_path);

        const char * vs_src[] = { vs.buffer };
        const char * fs_src[] = { fs.buffer };

        glShaderSource(vertex_shader, 1, vs_src, &vs.size);
        glShaderSource(fragment_shader, 1, fs_src, &fs.size);

        free_file(vs);
        free_file(fs);
    }

    compile_with_logs(vertex_shader);
    compile_with_logs(fragment_shader);

    GLuint shader_program = glCreateProgram();
    link_shader_with_logs(shader_program, vertex_shader, fragment_shader);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return shader_program;
}
