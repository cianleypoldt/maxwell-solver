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

// clang-format off
float main_quad_vertices[] = {
    -1, -1, -1, 1, 1, 1, -1, -1, 1, -1, 1, 1
};
// clang-format on
GLFWwindow *window_ptr;

GLuint volumetric_prog, vbo, vao;

int    current_texture = 0;
float *magnitude_buffer;
GLuint Etex, Btex;

simctx *sim;
int     window_width, window_height;

float aspect_ratio;
float camera_pos[3];
float camera_yaw;
float camera_pitch;

float max_ray_length;
int   max_steps;

GLint aspect_uniform_loc;
GLint camera_pos_uniform_loc;
GLint camera_yaw_uniform_loc;
GLint camera_pitch_uniform_loc;

static void resize_callback(GLFWwindow *window_ptr, int width, int height);

void init_renderer(simctx *ctx, int width, int height) {
    sim              = ctx;
    window_width     = width;
    window_height    = height;
    aspect_ratio     = window_width / (float) window_height;
    magnitude_buffer = malloc(sim->cell_count * sizeof(float));
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ptr = glfwCreateWindow(width, height, "floating", NULL, NULL);
    glfwSetInputMode(window_ptr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowSizeCallback(window_ptr, resize_callback);

    glfwMakeContextCurrent(window_ptr);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glViewport(0, 0, width, height);

    volumetric_prog = create_program("shaders/vol_vs.glsl", "shaders/vol_fs.glsl");

    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);
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

    aspect_uniform_loc       = glGetUniformLocation(volumetric_prog, "aspect");
    camera_pos_uniform_loc   = glGetUniformLocation(volumetric_prog, "camera_pos");
    camera_yaw_uniform_loc   = glGetUniformLocation(volumetric_prog, "camera_yaw");
    camera_pitch_uniform_loc = glGetUniformLocation(volumetric_prog, "camera_pitch");

    glUniform1f(glGetUniformLocation(volumetric_prog, "fovy"), 45.0f * (PI / 180));
    glUniform3f(glGetUniformLocation(volumetric_prog, "dimensions"), sim->Sx, sim->Sy, sim->Sz);
    glUniform3f(glGetUniformLocation(volumetric_prog, "voxel_size"), sim->dSx, sim->dSy, sim->dSz);
    glUniform1f(glGetUniformLocation(volumetric_prog, "max_ray_length"), 100.0f);
    glUniform1i(glGetUniformLocation(volumetric_prog, "max_steps"), 20);

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
    glUniform1f(aspect_uniform_loc, window_width / (float) window_height);
    glUniform3f(camera_pos_uniform_loc, camera_pos[0], camera_pos[1], camera_pos[2]);
    glUniform1f(camera_yaw_uniform_loc, camera_yaw);
    glUniform1f(camera_pitch_uniform_loc, camera_pitch);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, sizeof(main_quad_vertices) / (sizeof(float) * 2));
    glfwSwapBuffers(window_ptr);
}

int should_close() {
    return glfwWindowShouldClose(window_ptr) == GLFW_TRUE;
}

#define CAMERA_SPEED_VERTICAL   0.04
#define CAMERA_SPEED_HORIZONTAL 0.1

#define CAMERA_PITCH_SENSETIVITY 0.002
#define CAMERA_YAW_SENSETIVITY   0.002

int g_focused = 1;

void process_input() {
    {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window_ptr, GLFW_FOCUSED) == GLFW_TRUE && glfwGetMouseButton(window_ptr, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            glfwSetInputMode(window_ptr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            g_focused = 2;
        }
    }

    if (glfwGetKey(window_ptr, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetInputMode(window_ptr, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        g_focused = 0;
    }

    if (glfwGetKey(window_ptr, GLFW_KEY_SPACE) == GLFW_PRESS) {
        camera_pos[2] += CAMERA_SPEED_VERTICAL;
    }
    if (glfwGetKey(window_ptr, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        camera_pos[2] -= CAMERA_SPEED_VERTICAL;
    }

    float camera_forward[2] = { sin(camera_yaw), cos(camera_yaw) };

    if (glfwGetKey(window_ptr, GLFW_KEY_W) == GLFW_PRESS) {
        camera_pos[0] += camera_forward[0];
        camera_pos[1] += camera_forward[1];
    }
    if (glfwGetKey(window_ptr, GLFW_KEY_S) == GLFW_PRESS) {
        camera_pos[0] -= camera_forward[0];
        camera_pos[1] -= camera_forward[1];
    }

    if (glfwGetKey(window_ptr, GLFW_KEY_A) == GLFW_PRESS) {
        camera_pos[0] -= camera_forward[1];
        camera_pos[1] += camera_forward[0];
    }
    if (glfwGetKey(window_ptr, GLFW_KEY_D) == GLFW_PRESS) {
        camera_pos[0] += camera_forward[1];
        camera_pos[1] -= camera_forward[0];
    }
}

double prev_cursor_x, prev_cursor_y;

static void resize_callback(GLFWwindow *window_ptr, int width, int height) {
    glViewport(0, 0, width, height);
    window_width  = width;
    window_height = height;
}

static void curser_pos_callback(GLFWwindow *window_ptr, double xpos, double ypos) {
    if (!g_focused) {
        return;
    }

    double dx = 0;
    double dy = 0;

    if (g_focused == 1) {
        dx = xpos - prev_cursor_x;
        dy = ypos - prev_cursor_y;
    } else {
        g_focused = 1;
    }
    prev_cursor_x = xpos;
    prev_cursor_y = ypos;

    camera_pitch += -0.5 * dy * CAMERA_PITCH_SENSETIVITY;
    camera_yaw += -0.5 * dx * CAMERA_YAW_SENSETIVITY;
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
