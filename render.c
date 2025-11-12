#include "external/glad/glad.h"
#include "maxwell-solver.h"

#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

simctx     *sim    = NULL;
GLFWwindow *window = NULL;
int         w, h;

GLuint volumetric_shader_prog = 0;

GLint iTimeLocation       = 0;
GLint iResolutionLocation = 0;

GLuint ssbo = 0;

char  *readFile(const char *path);
GLuint create_shader_program(const char *vertexSrc, const char *fragmentSrc);
GLuint compile_shader(GLenum type, const char *source);

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}

void start_renderer(simctx *ctx) {
    if (!ctx) {
        fprintf(stderr, "renderer: Simulation context invalid");
    }
    sim = ctx;

    glfwInit();
    window = glfwCreateWindow(800, 600, "floating", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        fprintf(stderr, "renderer: Glad function loader failed");
    }

    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);

    char *vs_shader = readFile("shaders/vs_render_quad.glsl");
    char *fs_shader = readFile("shaders/fs_gpt.glsl");

    volumetric_shader_prog = create_shader_program(vs_shader, fs_shader);

    free(vs_shader);
    free(fs_shader);

    glUseProgram(volumetric_shader_prog);

    iTimeLocation       = glGetUniformLocation(volumetric_shader_prog, "iTime");
    iResolutionLocation = glGetUniformLocation(volumetric_shader_prog, "iResolution");

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, ctx->cell_count * BYTES_PER_CELL, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_DYNAMIC_STORAGE_BIT);

    void *mapped = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, ctx->cell_count * BYTES_PER_CELL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
}

void quit_renderer() {
    glDeleteProgram(volumetric_shader_prog);
    glfwDestroyWindow(window);
    glfwTerminate();
}

void clear_screen() {
    glClear(GL_COLOR_BUFFER_BIT);
}

int should_close() {
    glfwPollEvents();
    return glfwWindowShouldClose(window) || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
}

void update() {
    float time = (float) glfwGetTime();
    int   width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glUniform1f(iTimeLocation, time);
    glUniform2f(iResolutionLocation, (float) width, (float) height);
    // ensure shader reads the new data
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
}

void draw() {
    update();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glfwSwapBuffers(window);
}

char *readFile(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *) malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory for file: %s\n", path);
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';  // null-terminate
    fclose(file);
    return buffer;
}

GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // Check compile status
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        const char *typeName = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        fprintf(stderr, "ERROR::SHADER::%s::COMPILATION_FAILED\n%s\n", typeName, infoLog);
    }

    return shader;
}

GLuint create_shader_program(const char *vertexSrc, const char *fragmentSrc) {
    GLuint vertexShader   = compile_shader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, fragmentSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check link status
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        fprintf(stderr, "ERROR::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
    }

    // Shaders can be deleted after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}
