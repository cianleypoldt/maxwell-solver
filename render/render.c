#include "render.h"

#include "field.h"
#include "glad/glad.h"
#include "simulation.h"
#include <GLFW/glfw3.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

typedef struct {
    float x, y, z;
} vec3;

struct window {
    GLFWwindow *ptr;

    int width, height;

    enum {
        CURSOR_CAPTURED,
        CURSOR_FREE
    } cursor_state;

    int curs_px, curs_py;
    int curs_vx, curs_vy;
};

struct camera {
    float fov_y;
    float aspect;
    float near_plane;
    float far_plane;

    float pos[3];
    float rot[4];

    float proj[16];
    float view[16];
};

static void camera_build_view(struct camera *camera) {
    mat_identity(camera->view, 4);

    // translation
    camera->view[3 + 0 * 4] = -camera->pos[0];
    camera->view[3 + 1 * 4] = -camera->pos[1];
    camera->view[3 + 2 * 4] = -camera->pos[2];

    float conj_rot[4];
    quat_conj(conj_rot, camera->rot);

    float M[16];
    mat4_from_quat(M, conj_rot);

    mat_mul(camera->view, M, camera->view, 4, 4, 4);
}

static void camera_build_proj(struct camera *camera) {
    // axis symmetric proection
    float *M = camera->proj;

    const float near = camera->near_plane;
    const float far = camera->far_plane;

    const float top = tanf(camera->fov_y / 2.0f) * camera->near_plane;
    const float bottom = -top;
    const float right = top * camera->aspect;
    const float left = -right;

    memset(M, 0, sizeof(float) * 16);
    // screenspace x
    M[0 + 0 * 4] = 2 * near / (right - left);
    M[0 + 2 * 4] = (right + left) / (right - left);
    // screenspace y
    M[1 + 1 * 4] = 2 * near / (top - bottom);
    M[1 + 2 * 4] = (top + bottom) / (top - bottom);
    // normalized depth
    M[2 + 2 * 4] = -(far + near) / (far - near);
    M[2 + 3 * 4] = -2 * far * near / (far - near);
    M[3 + 2 * 4] = -1;
}

struct vol_uniform_locations {
    GLint camera_pos, camera_forward, camera_right, camera_up, intensity_E_field, intensity_B_field;
};

struct uniform_locations {
    GLint proj, view, light_angle;
};

struct renderctx {
    const em_field *field;

    struct window window;
    struct camera camera;

    GLuint vol_vbo, vol_vao;
    GLuint vol_pathtracer_prog;
    GLuint vol_Etex, vol_Btex;
    float vol_step_size;

    struct vol_uniform_locations vol_uniforms;
    struct uniform_locations uniforms;

    GLuint vbo, vao;
} renderer;

static void init_camera() {
    struct camera *c = &renderer.camera;
    memset(c, 0, sizeof(*c));

    c->fov_y = 45 * (M_PI / 180);
    c->aspect = (float)renderer.window.width / renderer.window.height;
    c->near_plane = 1.0f;
    c->near_plane = 100.0f;

    c->rot[3] = 1.0f;

    camera_build_proj(c);
    camera_build_view(c);
}

static void window_resize_callback(GLFWwindow *window, int width, int height) {
    static struct window *w = &renderer.window;
    glViewport(0, 0, width, height);
    w->width = width;
    w->height = height;

    renderer.camera.aspect = (float)width / height;
    camera_build_proj(&renderer.camera);
}

static void cursor_pos_callback(GLFWwindow *window, double x, double y) {
    static struct window *w = &renderer.window;
    w->curs_vx = x - w->curs_px;
    w->curs_vy = y - w->curs_py;
    w->curs_px = x;
    w->curs_py = y;
}

static int init_window(int width, int height, char *name) {
    static struct window *w = &renderer.window;

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    w->ptr = glfwCreateWindow(width, height, name, NULL, NULL);
    if (!w->ptr) return -1;

    glfwSetInputMode(w->ptr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowSizeCallback(w->ptr, window_resize_callback);
    glfwSetCursorPosCallback(w->ptr, cursor_pos_callback);

    glfwMakeContextCurrent(w->ptr);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glfwGetWindowSize(w->ptr, &w->width, &w->height);
    glViewport(0, 0, w->width, w->height);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

    return 0;
}

static void destroy_window() {
    glfwDestroyWindow(renderer.window.ptr);
    glfwTerminate();
}

GLuint create_program(const char *vs_path, const char *fs_path);

int init_renderer(simctx *ctx) {
    memset(&renderer, 0, sizeof(struct renderctx));

    if (!ctx) return -1;
    renderer.field = get_field(ctx);

    if (init_window(800, 600, "floating") < 0) return -1;

    // Geometry
    glGenBuffers(1, &renderer.vbo);
    glGenVertexArrays(1, &renderer.vao);

    glBindBuffer(GL_ARRAY_BUFFER, renderer.vbo);
    glBindVertexArray(renderer.vao);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Volumetric
    // Screenspace quad for path tracing shader
    glGenBuffers(1, &renderer.vol_vbo);
    glGenVertexArrays(1, &renderer.vol_vao);

    glBindBuffer(GL_ARRAY_BUFFER, renderer.vol_vbo);
    glBindVertexArray(renderer.vol_vao);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    {
        float fullscreen_quad_vertices[] = {-1, -1, -1, 1, 1, 1, -1, -1, 1, -1, 1, 1};
        glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreen_quad_vertices), fullscreen_quad_vertices, GL_STATIC_DRAW);
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // 3d field component textures for path tracing shader
    glGenTextures(1, &renderer.vol_Etex);
    glGenTextures(1, &renderer.vol_Btex);
    GLuint tex3d_objects[2] = {renderer.vol_Etex, renderer.vol_Btex};
    for (int i = 0; i < 2; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_3D, tex3d_objects[i]);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, renderer.field->Nz, renderer.field->Ny, renderer.field->Nx, 0, GL_RED, GL_FLOAT, NULL);
    }
    glBindTexture(GL_TEXTURE_3D, 0);

    // volumetric shader program and static uniforms
    renderer.vol_pathtracer_prog = create_program("render/shaders/vol_vs.glsl", "render/shaders/vol_fs.glsl");
    glUseProgram(renderer.vol_pathtracer_prog);
    glUniform1i(glGetUniformLocation(renderer.vol_pathtracer_prog, "Etex"), 0);
    glUniform1i(glGetUniformLocation(renderer.vol_pathtracer_prog, "Btex"), 1);
    glUniform3f(glGetUniformLocation(renderer.vol_pathtracer_prog, "voxel_size"), renderer.field->dSx, renderer.field->dSy, renderer.field->dSz);
    glUniform1f(glGetUniformLocation(renderer.vol_pathtracer_prog, "step_size"), renderer.vol_step_size);
    glUniform3f(
        glGetUniformLocation(renderer.vol_pathtracer_prog, "dimensions"),
        get_em_field_width(renderer.field),
        get_em_field_height(renderer.field),
        get_em_field_depth(renderer.field)
    );
    glUseProgram(0);

    // volumetric shader dynamic uniforms
    struct vol_uniform_locations *vds = &renderer.vol_uniforms;
    vds->camera_pos = glGetUniformLocation(renderer.vol_pathtracer_prog, "camera_pos");
    vds->camera_forward = glGetUniformLocation(renderer.vol_pathtracer_prog, "camera_forward");
    vds->camera_right = glGetUniformLocation(renderer.vol_pathtracer_prog, "camera_right");
    vds->camera_up = glGetUniformLocation(renderer.vol_pathtracer_prog, "camera_up");
    vds->intensity_E_field = glGetUniformLocation(renderer.vol_pathtracer_prog, "intensity_E_field");
    vds->intensity_B_field = glGetUniformLocation(renderer.vol_pathtracer_prog, "intensity_B_field");

    printf("GL init error: %x\n", glGetError());

    return 0;
}

void deinit_renderer() {
    glDeleteVertexArrays(1, &renderer.vao);
    glDeleteBuffers(1, &renderer.vbo);

    glDeleteTextures(1, &renderer.vol_Etex);
    glDeleteTextures(1, &renderer.vol_Btex);
    glDeleteVertexArrays(1, &renderer.vol_vao);
    glDeleteBuffers(1, &renderer.vol_vbo);
    glDeleteProgram(renderer.vol_pathtracer_prog);
    printf("GL Error: %x\n", glGetError());

    destroy_window();
}

void render_current() {
    glfwSwapBuffers(renderer.window.ptr);
    // Geometry
    // bind shader, accumulation, update uniforms, vao
    // draw geometry, collect depth and accumulation
    //
    // Volumme
    // bind shader, accumulation, depth, textures, update uniforms, vao
    // render volume over geometry

    return;
}

void process_input() {
    glfwPollEvents();
    return;
}

int should_close() {
    return glfwWindowShouldClose(renderer.window.ptr);
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
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    {
        struct file vs = load_file(vs_path);
        struct file fs = load_file(fs_path);

        const char *vs_src[] = {vs.buffer};
        const char *fs_src[] = {fs.buffer};

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
