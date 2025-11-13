#include "external/glad/glad.h"
#include "maxwell-solver.h"

#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

// max field magnitudes for color mapping
const double E_max = 1;
const double B_max = 1;

typedef struct VoxelData {
    float magE2, magB2;
    float eps, mu;
} voxel_data;

simctx     *sim    = NULL;
GLFWwindow *window = NULL;
int         w, h;

GLuint volumetric_shader_prog = 0;

GLint iTimeLocation       = 0;
GLint iResolutionLocation = 0;

GLuint ssbo = 0;

voxel_data *voxels = NULL;

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

    iTimeLocation = glGetUniformLocation(volumetric_shader_prog, "iTime");
    iResolutionLocation =
        glGetUniformLocation(volumetric_shader_prog, "iResolution");

    voxels = malloc(ctx->cell_count);
    for (int i = 0; i < ctx->cell_count; i++) {
        voxels[i].eps = ctx->eps[i];
    }
    for (int i = 0; i < ctx->cell_count; i++) {
        voxels[i].mu = ctx->mu[i];
    }

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, ctx->cell_count * sizeof(voxel_data),
                 voxels, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
}

void quit_renderer() {
    if (voxels) {
        free(voxels);
    }
    glDeleteProgram(volumetric_shader_prog);
    glfwDestroyWindow(window);
    glfwTerminate();
}

void clear_screen() {
    glClear(GL_COLOR_BUFFER_BIT);
}

int should_close() {
    glfwPollEvents();
    return glfwWindowShouldClose(window) ||
           glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
}

void lattice_to_buffer(simctx *ctx) {
    double *field = ctx->Ex;
    double  emax2 = E_max * E_max;  // normalize color values for color mapping
    for (int component = 0; component < 3; component++) {
        for (int i = 0; i < ctx->cell_count; i++) {
            voxels[i].magE2 += (*field + *(field++)) / emax2;
        }
    }
    double bmax2 = B_max * B_max;
    for (int component = 0; component < 3; component++) {
        for (int i = 0; i < ctx->cell_count; i++) {
            voxels[i].magB2 += (*field + *(field++)) / bmax2;
        }
    }
}

void update() {
    float time = (float) glfwGetTime();
    int   width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glUniform1f(iTimeLocation, time);
    glUniform2f(iResolutionLocation, (float) width, (float) height);
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
        const char *typeName =
            (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        fprintf(stderr, "ERROR::SHADER::%s::COMPILATION_FAILED\n%s\n", typeName,
                infoLog);
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
