#include "render.h"

#include "fdtd.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846

GLuint create_program(const char *vs_path, const char *fs_path);

float main_quad_vertices[] = {
    -1, -1, 0, 0, 0, 1, -1, 0, 1, 0, 1, 1, 0, 1, 1, -1, -1, 0, 0, 0, 1, 1, 0, 1, 1, -1, 1, 0, 0, 1,
};

GLFWwindow *window_ptr;

GLuint volumetric_prog, vbo, vao;

int    current_texture = 0;
float *magnitude_buffer;
GLuint Etex, Btex;

simctx *sim;
int     window_width, window_height;

float camera_pos[3];
float camera_yaw;
float camera_pitch;

float max_ray_length;
int   max_steps;

void init_renderer(simctx *ctx, int width, int height) {
    sim              = ctx;
    width            = width;
    height           = height;
    magnitude_buffer = malloc(sim->cell_count * sizeof(float));
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ptr = glfwCreateWindow(width, height, "floating", NULL, NULL);
    glfwSetInputMode(window_ptr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwMakeContextCurrent(window_ptr);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glViewport(0, 0, width, height);

    volumetric_prog = create_program("shaders/vol_vs.glsl", "shaders/vol_fs.glsl");

    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBufferData(GL_ARRAY_BUFFER, sizeof(main_quad_vertices), main_quad_vertices, GL_STATIC_DRAW);

    glGenTextures(1, &Etex);
    glGenTextures(1, &Btex);

    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_3D, Etex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_3D, Btex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glUseProgram(volumetric_prog);
    glUniform1i(glGetUniformLocation(volumetric_prog, "Etex"), 0);
    glUniform1i(glGetUniformLocation(volumetric_prog, "Btex"), 1);

    glUniform1f(glGetUniformLocation(volumetric_prog, "fovy"), 45.0f * (PI / 180));
    glUniform1f(glGetUniformLocation(volumetric_prog, "aspect"), width / (float) height);
    glUniform1f(glGetUniformLocation(volumetric_prog, "camera_yaw"), camera_yaw);
    glUniform1f(glGetUniformLocation(volumetric_prog, "camera_pitch"), camera_pitch);
    glUniform3f(glGetUniformLocation(volumetric_prog, "camera_pos"), camera_pos[0], camera_pos[1], camera_pos[2]);
    glUniform1f(glGetUniformLocation(volumetric_prog, "max_ray_length"), 100.0f);
    glUniform1i(glGetUniformLocation(volumetric_prog, "max_steps"), 20);

    float min_xyz[3] = { -sim->Sx / 2.0f, -sim->Sy / 2.0f, -sim->Sz / 2.0f };
    float max_xyz[3] = { sim->Sx / 2.0f, sim->Sy / 2.0f, sim->Sz / 2.0f };
    glUniform3f(glGetUniformLocation(volumetric_prog, "min_xyz"), min_xyz[0], min_xyz[1], min_xyz[2]);
    glUniform3f(glGetUniformLocation(volumetric_prog, "max_xyz"), max_xyz[0], max_xyz[1], max_xyz[2]);
    glUniform3f(glGetUniformLocation(volumetric_prog, "voxel_size"), sim->dSx, sim->dSy, sim->dSz);

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(window_ptr);
}

void deinit_renderer() {
    glDeleteTextures(1, &Etex);
    glDeleteTextures(1, &Btex);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(volumetric_prog);
    glfwTerminate();
}

static void buffer_components(float *F[3], GLuint texture) {
    for (int i = 0; i < sim->Nx; i++) {
        for (int j = 0; j < sim->Nz; j++) {
            int idx = i * sim->stride_x + j * sim->stride_y + 1 * sim->stride_z;
            for (int k = 0; k < sim->Nz; k++) {
                magnitude_buffer[idx] = sqrtf(F[0][idx] * F[0][idx] + F[1][idx] * F[1][idx] + F[2][idx] * F[2][idx]);
                idx++;
            }
        }
    }
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, sim->Nx, sim->Ny, sim->Nz, GL_RED, GL_FLOAT, magnitude_buffer);
}

void render_current() {
    buffer_components((float *[3]){ sim->Ex, sim->Ey, sim->Ez }, Etex);
    buffer_components((float *[3]){ sim->Hx, sim->Hy, sim->Hz }, Btex);

    glUseProgram(volumetric_prog);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, sizeof(main_quad_vertices) / (sizeof(float) * 3));
    glfwSwapBuffers(window_ptr);
}

struct file {
    char *buffer;
    int   size;
};

static struct file load_file(const char *path) {
    FILE *fp = fopen(path, "rb");

    if (!fp) {
        return (struct file){ NULL, 0 };
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0) {
        fclose(fp);
        return (struct file){ NULL, 0 };
    }
    void *buffer = malloc(size);
    if (!buffer) {
        fclose(fp);
        return (struct file){ NULL, 0 };
    }

    fread(buffer, 1, size, fp);
    fclose(fp);

    return (struct file){ buffer, size };
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
    char *log = malloc(len + 1);
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
    char *log = malloc(len ? len : 1);
    glGetProgramInfoLog(prog, len, NULL, log);
    printf("Failed to link shaders: \n");
    printf("%s\n", log);
    free(log);

    return 0;
}

GLuint create_program(const char *vs_path, const char *fs_path) {
    GLuint vertex_shader   = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    {
        struct file vs = load_file(vs_path);
        struct file fs = load_file(fs_path);

        const char *vs_src[] = { vs.buffer };
        const char *fs_src[] = { fs.buffer };

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
