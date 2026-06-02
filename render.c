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

static void vec3_norm(float* res, float* v);
static void vec3_scale(float* res, float* v, float s);
static void vec3_cross(float* res, float* v1, float* v2);

GLuint create_program(const char* vs_path, const char* fs_path);

float main_quad_vertices[] = {-1, -1, -1, 1, 1, 1, -1, -1, 1, -1, 1, 1};

float* magnitude_buffer;

typedef struct {
    float pos[3];
    float yaw;
    float pitch;

    float forward[3];
    float up[3];
    float right[3];

    float aspect_ratio;
    float fovy;
} camera_t;

// at least 1 bug!
static void camera_update_directionals(camera_t* camera) {
    camera->forward[0] = cosf(camera->pitch) * sinf(camera->yaw);
    camera->forward[1] = cosf(camera->pitch) * cosf(camera->yaw);
    camera->forward[2] = sinf(camera->pitch);
    vec3_norm(camera->forward, camera->forward);

    camera->right[0] = -camera->forward[1];
    camera->right[1] = camera->forward[0];
    camera->right[2] = 0;
    vec3_norm(camera->right, camera->right);

    vec3_cross(camera->up, camera->right, camera->forward);

    vec3_scale(camera->right, camera->right, 1);
    vec3_scale(camera->up, camera->up, 1);
}

struct renderer {
    simctx* sim;

    GLFWwindow* window_ptr;
    int window_width, window_height;
    camera_t camera;
    float max_ray_length;
    int max_steps;

    GLuint volumetric_prog, vbo, vao;
    GLuint Etex, Btex;
    GLint camera_pos_uniform_loc;
    GLint camera_forward_uniform_loc;
    GLint camera_right_uniform_loc;
    GLint camera_up_uniform_loc;
} renderer;

static void resize_callback(GLFWwindow* window_ptr, int width, int height);
static void curser_pos_callback(GLFWwindow* window_ptr, double xpos, double ypos);

void renderer_init(simctx* ctx, int width, int height) {
    memset(&renderer, 0, sizeof(renderer));

    renderer.sim = ctx;
    renderer.window_width = width;
    renderer.window_height = height;

    renderer.camera.aspect_ratio = renderer.window_width / (float)renderer.window_height;
    renderer.camera.fovy = 50;

    magnitude_buffer = malloc(renderer.sim->cell_count * sizeof(float));

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    renderer.window_ptr = glfwCreateWindow(width, height, "floating", NULL, NULL);
    glfwSetInputMode(renderer.window_ptr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowSizeCallback(renderer.window_ptr, resize_callback);
    glfwSetCursorPosCallback(renderer.window_ptr, curser_pos_callback);

    glfwMakeContextCurrent(renderer.window_ptr);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glViewport(0, 0, width, height);

    renderer.volumetric_prog = create_program("shaders/vol_vs.glsl", "shaders/vol_fs.glsl");

    glGenBuffers(1, &renderer.vbo);
    glGenVertexArrays(1, &renderer.vao);
    glBindVertexArray(renderer.vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer.vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(main_quad_vertices), main_quad_vertices, GL_STATIC_DRAW);

    glGenTextures(1, &renderer.Etex);
    glGenTextures(1, &renderer.Btex);

    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_3D, renderer.Etex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_3D, renderer.Btex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glUseProgram(renderer.volumetric_prog);
    glUniform1i(glGetUniformLocation(renderer.volumetric_prog, "Etex"), 0);
    glUniform1i(glGetUniformLocation(renderer.volumetric_prog, "Btex"), 1);

    glUniform3f(glGetUniformLocation(renderer.volumetric_prog, "voxel_size"), renderer.sim->dSx, renderer.sim->dSy, renderer.sim->dSz);
    glUniform3f(glGetUniformLocation(renderer.volumetric_prog, "dimensions"), renderer.sim->Sx, renderer.sim->Sy, renderer.sim->Sz);
    glUniform1i(glGetUniformLocation(renderer.volumetric_prog, "max_steps"), 20);
    glUniform1f(glGetUniformLocation(renderer.volumetric_prog, "max_ray_length"), 100.0f);

    renderer.camera_pos_uniform_loc = glGetUniformLocation(renderer.volumetric_prog, "camera_pos");
    renderer.camera_forward_uniform_loc = glGetUniformLocation(renderer.volumetric_prog, "camera_forward");
    renderer.camera_right_uniform_loc = glGetUniformLocation(renderer.volumetric_prog, "camera_right");
    renderer.camera_up_uniform_loc = glGetUniformLocation(renderer.volumetric_prog, "camera_up");

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(renderer.window_ptr);
}

void renderer_deinit() {
    glDeleteTextures(1, &renderer.Etex);
    glDeleteTextures(1, &renderer.Btex);
    glDeleteBuffers(1, &renderer.vbo);
    glDeleteVertexArrays(1, &renderer.vao);
    glDeleteProgram(renderer.volumetric_prog);

    glfwTerminate();
}

static void buffer_components(float* F[3], GLuint texture) {
    for (int i = 0; i < renderer.sim->Nx; i++) {
        for (int j = 0; j < renderer.sim->Nz; j++) {
            int idx = i * renderer.sim->stride_x + j * renderer.sim->stride_y + 1 * renderer.sim->stride_z;
            for (int k = 0; k < renderer.sim->Nz; k++) {
                magnitude_buffer[idx] = sqrtf(F[0][idx] * F[0][idx] + F[1][idx] * F[1][idx] + F[2][idx] * F[2][idx]);
                idx++;
            }
        }
    }
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, renderer.sim->Nx, renderer.sim->Ny, renderer.sim->Nz, GL_RED, GL_FLOAT, magnitude_buffer);
}

void render_current() {
    buffer_components((float* [3]){renderer.sim->Ex, renderer.sim->Ey, renderer.sim->Ez}, renderer.Etex);
    buffer_components((float* [3]){renderer.sim->Hx, renderer.sim->Hy, renderer.sim->Hz}, renderer.Btex);

    process_input();
    camera_update_directionals(&renderer.camera);

    glUseProgram(renderer.volumetric_prog);
    glUniform3f(renderer.camera_pos_uniform_loc, renderer.camera.pos[0], renderer.camera.pos[1], renderer.camera.pos[2]);
    glUniform3f(renderer.camera_forward_uniform_loc, renderer.camera.forward[0], renderer.camera.forward[1], renderer.camera.forward[2]);
    glUniform3f(renderer.camera_right_uniform_loc, renderer.camera.right[0], renderer.camera.right[1], renderer.camera.right[2]);
    glUniform3f(renderer.camera_up_uniform_loc, renderer.camera.up[0], renderer.camera.up[1], renderer.camera.up[2]);

    glBindVertexArray(renderer.vao);
    glDrawArrays(GL_TRIANGLES, 0, sizeof(main_quad_vertices) / (sizeof(float) * 2));
    glfwSwapBuffers(renderer.window_ptr);
}

int should_close() {
    return glfwWindowShouldClose(renderer.window_ptr) == GLFW_TRUE;
}

#define CAMERA_SPEED_VERTICAL   0.04
#define CAMERA_SPEED_HORIZONTAL 0.1

#define CAMERA_PITCH_SENSETIVITY 0.02
#define CAMERA_YAW_SENSETIVITY   0.02

int g_focused = 1;

void process_input() {
    {
        glfwPollEvents();
        if (glfwGetWindowAttrib(renderer.window_ptr, GLFW_FOCUSED) == GLFW_TRUE &&
            glfwGetMouseButton(renderer.window_ptr, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            glfwSetInputMode(renderer.window_ptr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            g_focused = 2;
        }
    }

    if (glfwGetKey(renderer.window_ptr, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetInputMode(renderer.window_ptr, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        g_focused = 0;
    }

    if (glfwGetKey(renderer.window_ptr, GLFW_KEY_SPACE) == GLFW_PRESS) {
        renderer.camera.pos[2] += CAMERA_SPEED_VERTICAL;
    }
    if (glfwGetKey(renderer.window_ptr, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        renderer.camera.pos[2] -= CAMERA_SPEED_VERTICAL;
    }

    float camera_forward[2] = {sin(renderer.camera.yaw), cos(renderer.camera.yaw)};

    if (glfwGetKey(renderer.window_ptr, GLFW_KEY_W) == GLFW_PRESS) {
        renderer.camera.pos[0] += camera_forward[0];
        renderer.camera.pos[1] += camera_forward[1];
    }
    if (glfwGetKey(renderer.window_ptr, GLFW_KEY_S) == GLFW_PRESS) {
        renderer.camera.pos[0] -= camera_forward[0];
        renderer.camera.pos[1] -= camera_forward[1];
    }

    if (glfwGetKey(renderer.window_ptr, GLFW_KEY_A) == GLFW_PRESS) {
        renderer.camera.pos[0] -= camera_forward[1];
        renderer.camera.pos[1] += camera_forward[0];
    }
    if (glfwGetKey(renderer.window_ptr, GLFW_KEY_D) == GLFW_PRESS) {
        renderer.camera.pos[0] += camera_forward[1];
        renderer.camera.pos[1] -= camera_forward[0];
    }
}

double prev_cursor_x, prev_cursor_y;

static void resize_callback(GLFWwindow* window_ptr, int width, int height) {
    glViewport(0, 0, width, height);
    renderer.window_width = width;
    renderer.window_height = height;
}

static void curser_pos_callback(GLFWwindow* window_ptr, double xpos, double ypos) {
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

    renderer.camera.pitch += 0.5 * dy * CAMERA_PITCH_SENSETIVITY;
    renderer.camera.yaw += -0.5 * dx * CAMERA_YAW_SENSETIVITY;
}

static void vec3_norm(float* res, float* v) {
    float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (fabs(len) <= 0.0001) return;
    res[0] = v[0] / len;
    res[1] = v[1] / len;
    res[2] = v[2] / len;
}

static void vec3_scale(float* res, float* v, float s) {
    res[0] = v[0] * s;
    res[1] = v[1] * s;
    res[2] = v[2] * s;
}

static void vec3_cross(float* res, float* v1, float* v2) {
    float tmp[3];
    tmp[0] = v1[1] * v2[2] - v1[2] * v2[1];
    tmp[1] = v1[2] * v2[0] - v1[0] * v2[2];
    tmp[2] = v1[0] * v2[1] - v1[1] * v2[0];
    memcpy(res, tmp, sizeof(float) * 3);
}

struct file {
    char* buffer;
    int size;
};

static struct file load_file(const char* path) {
    FILE* fp = fopen(path, "rb");

    if (!fp) {
        return (struct file){NULL, 0};
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0) {
        fclose(fp);
        return (struct file){NULL, 0};
    }
    void* buffer = malloc(size);
    if (!buffer) {
        fclose(fp);
        return (struct file){NULL, 0};
    }

    fread(buffer, 1, size, fp);
    fclose(fp);

    return (struct file){buffer, size};
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
    char* log = malloc(len + 1);
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
    char* log = malloc(len ? len : 1);
    glGetProgramInfoLog(prog, len, NULL, log);
    printf("Failed to link shaders: \n");
    printf("%s\n", log);
    free(log);

    return 0;
}

GLuint create_program(const char* vs_path, const char* fs_path) {
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    {
        struct file vs = load_file(vs_path);
        struct file fs = load_file(fs_path);

        const char* vs_src[] = {vs.buffer};
        const char* fs_src[] = {fs.buffer};

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
