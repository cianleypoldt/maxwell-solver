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

static void camera_build_proj(struct camera *camera) {
    // axis symmetric proection
    // TODO: simplify generic projection math maybe
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

static void camera_build_view(struct camera *camera) {
    mat_identity(camera->view, 4);

    // translation
    camera->view[0 + 3 * 4] = -camera->pos[0];
    camera->view[1 + 3 * 4] = -camera->pos[1];
    camera->view[2 + 3 * 4] = -camera->pos[2];

    float conj_rot[4];
    quat_conj(conj_rot, camera->rot);

    float M[16];
    mat4_from_quat(M, conj_rot);

    mat_mul(camera->view, M, camera->view, 4, 4, 4);
}

struct vol_uniform_locations {
    GLint camera_pos, camera_forward, camera_right, camera_up, intensity_E_field, intensity_B_field;
};

struct uniform_locations {
    GLint proj, view, light_angle;
};

float *magnitude_buffer;

struct renderctx {
    const em_field *field;

    struct window window;
    struct camera camera;

    GLuint vbo, vao;
    GLuint shader_prog;
    struct uniform_locations uniforms;

    GLuint vol_vbo, vol_vao;
    GLuint vol_pathtracer_prog;
    GLuint vol_Etex, vol_Btex;
    float vol_step_size;
    struct vol_uniform_locations vol_uniforms;

} renderer;

static void init_camera() {
    struct camera *c = &renderer.camera;
    memset(c, 0, sizeof(*c));

    c->fov_y = 45 * (M_PI / 180);
    c->aspect = (float)renderer.window.width / renderer.window.height;
    c->near_plane = 1.0f;
    c->near_plane = 100.0f;

    c->rot[0] = 1.0f;

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
    struct window *w = &renderer.window;

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

    glfwSetInputMode(w->ptr, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

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

    if (init_window(1400, 800, "floating") < 0) return -1;

    // Geometry
    glGenBuffers(1, &renderer.vbo);
    glGenVertexArrays(1, &renderer.vao);

    glBindBuffer(GL_ARRAY_BUFFER, renderer.vbo);
    glBindVertexArray(renderer.vao);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    {
        float fullscreen_quad_vertices[] = {-0.5f, -0.5f, -2.0f, 0.5f, -0.5f, -2.0f, 0.0f, 0.5f, -2.0f};
        glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreen_quad_vertices), fullscreen_quad_vertices, GL_STATIC_DRAW);
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    renderer.shader_prog = create_program("render/shaders/vs.glsl", "render/shaders/fs.glsl");
    {
        struct uniform_locations *u = &renderer.uniforms;
        GLuint prog = renderer.shader_prog;
        glUseProgram(prog);
        u->proj = glGetUniformLocation(prog, "proj");
        u->view = glGetUniformLocation(prog, "view");
        glUseProgram(0);
    }

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
    {
        // volumetric shader dynamic uniforms
        struct vol_uniform_locations *vds = &renderer.vol_uniforms;
        GLuint prog = renderer.vol_pathtracer_prog;
        glUseProgram(prog);
        vds->camera_pos = glGetUniformLocation(prog, "camera_pos");
        vds->camera_forward = glGetUniformLocation(prog, "camera_forward");
        vds->camera_right = glGetUniformLocation(prog, "camera_right");
        vds->camera_up = glGetUniformLocation(prog, "camera_up");
        vds->intensity_E_field = glGetUniformLocation(prog, "intensity_E_field");
        vds->intensity_B_field = glGetUniformLocation(prog, "intensity_B_field");
        glUseProgram(0);
    }

    printf("GL init error: %x\n", glGetError());

    magnitude_buffer = malloc(get_em_field_cell_count(renderer.field) * sizeof(float));

    init_camera();

    return 0;
}

void deinit_renderer() {
    glDeleteBuffers(1, &renderer.vbo);
    glDeleteVertexArrays(1, &renderer.vao);
    glDeleteProgram(renderer.shader_prog);

    glDeleteTextures(1, &renderer.vol_Etex);
    glDeleteTextures(1, &renderer.vol_Btex);
    glDeleteVertexArrays(1, &renderer.vol_vao);
    glDeleteBuffers(1, &renderer.vol_vbo);
    glDeleteProgram(renderer.vol_pathtracer_prog);
    printf("GL Error: %x\n", glGetError());
    free(magnitude_buffer);

    destroy_window();
}

static void buffer_components(float *restrict Fx, float *restrict Fy, float *restrict Fz, GLuint texture) {
    for (int i = 0; i < renderer.field->Nx; i++) {
        for (int j = 0; j < renderer.field->Ny; j++) {
            int idx = i * renderer.field->stride_x + j * renderer.field->stride_y;
            for (int k = 0; k < renderer.field->Nz; k++) {
                magnitude_buffer[idx] = sqrtf(Fx[idx] * Fx[idx] + Fy[idx] * Fy[idx] + Fz[idx] * Fz[idx]);

                //magnitude_buffer[idx] = (Fx[idx] - Fx[idx - renderer.sim->stride_x]) / renderer.sim->dSx +
                //                        (Fy[idx] - Fy[idx - renderer.sim->stride_y]) / renderer.sim->dSy +
                //                        (Fz[idx] - Fz[idx - renderer.sim->stride_z]) / renderer.sim->dSz;
                //magnitude_buffer[idx] *= 0.01;

                // magnitude_buffer[idx] = renderer.sim->Sigma[idx] * 0.001;

                idx++;
            }
        }
    }
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, renderer.field->Nz, renderer.field->Ny, renderer.field->Nx, GL_RED, GL_FLOAT, magnitude_buffer);
}

void render_current() {
    // Geometry
    // bind shader, accumulation, update uniforms, vao
    // draw geometry, collect depth and accumulation

    {
        struct uniform_locations *u = &renderer.uniforms;
        GLuint prog = renderer.shader_prog;
        glUseProgram(prog);
        glUniformMatrix4fv(u->proj, 1, GL_FALSE, renderer.camera.proj);
        glUniformMatrix4fv(u->view, 1, GL_FALSE, renderer.camera.view);
    }

    glBindVertexArray(renderer.vao);

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glfwSwapBuffers(renderer.window.ptr);

    // Volumme
    // bind shader, accumulation, depth, textures, update uniforms, vao
    // render volume over geometry

    return;
}

#define CAMERA_SPEED_HORIZONTAL 0.2
#define CAMERA_SPEED_VERTICAL   0.1

void process_input() {
    // TODO: Rewrite everything

    glfwPollEvents();

    struct window *w = &renderer.window;
    struct camera *c = &renderer.camera;

    if (glfwGetKey(w->ptr, GLFW_KEY_SPACE) == GLFW_PRESS) {
        c->pos[1] += CAMERA_SPEED_VERTICAL;
    }
    if (glfwGetKey(w->ptr, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        c->pos[1] -= CAMERA_SPEED_VERTICAL;
    }

    float q_camera_forward[4] = {0, 0, 0, -1};
    quat_sandwitch(q_camera_forward, q_camera_forward, c->rot);

    float *camera_forward = q_camera_forward + 1;
    camera_forward[1] = 0;  // horizontal component
    vec_normalize(camera_forward, camera_forward, 3);
    vec_scale(camera_forward, camera_forward, CAMERA_SPEED_HORIZONTAL, 3);

    if (glfwGetKey(w->ptr, GLFW_KEY_W) == GLFW_PRESS) {
        vec_add(c->pos, c->pos, camera_forward, 3);
    }
    if (glfwGetKey(w->ptr, GLFW_KEY_S) == GLFW_PRESS) {
        vec_sub(c->pos, c->pos, camera_forward, 3);
    }
    float camera_left[3];
    camera_left[0] = camera_forward[2];
    camera_left[1] = 0;
    camera_left[2] = -camera_forward[0];

    if (glfwGetKey(w->ptr, GLFW_KEY_A) == GLFW_PRESS) {
        vec_add(c->pos, c->pos, camera_left, 3);
    }
    if (glfwGetKey(w->ptr, GLFW_KEY_D) == GLFW_PRESS) {
        vec_sub(c->pos, c->pos, camera_left, 3);
    }

    camera_build_view(c);
    mat_print(c->view, 4, 4);
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
