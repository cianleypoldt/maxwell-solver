#include "render.h"

#include "field.h"
#include "glad/glad.h"
#include "simulation.h"
#include <GLFW/glfw3.h>
#include <complex.h>
#include <math.h>
#include <stdint.h>
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

struct border_uniform_locations {
    GLint proj, view;
};

struct sources_uniform_locations {
    GLint proj, view, model, light_angle;
};

struct render_obj {
    GLuint vbo, ebo, vao, shader;
    int element_count;
};

#define N_SOURCES 10
const float source_positions[N_SOURCES][3] = {
    {0.0f, 0.0f, 0.0f},
    {2.0f, 5.0f, -15.0f},
    {-1.5f, -2.2f, -2.5f},
    {-3.8f, -2.0f, -12.3f},
    {2.4f, -0.4f, -3.5f},
    {-1.7f, 3.0f, -7.5f},
    {1.3f, -2.0f, -2.5f},
    {1.5f, 2.0f, -2.5f},
    {1.5f, 0.2f, -1.5f},
    {-1.3f, 1.0f, -1.5f}
};

struct renderctx {
    const em_field *field;

    struct window window;
    struct camera camera;

    // Shared
    GLuint scene_fbo, scene_color_tex, scene_depth_tex;

    // Borders, probably temp
    struct render_obj volume_borders;
    struct border_uniform_locations border_uniforms;

    // Sources, probably temp & to be included in general opaque abstraction
    struct render_obj sources;
    struct sources_uniform_locations sources_uniforms;

    // Opaque geometry abstraction:
    //   - requires a stronger shader and uniform abstraction
    //   - Mesh + shader + init (defaults or custom) + render (d.o.c.) + destroy (default as well as custom)

    // Volume, non temp, remains it's own pass
    struct render_obj volume_pathtrace;
    GLuint volume_fbo, volume_color_tex;
    GLuint vol_Etex, vol_Btex;
    float vol_step_size;
    struct vol_uniform_locations vol_uniforms;
    float *vol_magnitude_buffer;

    // transparent geometry (WOIT), yet to be implemented
    struct render_obj oit_geometry;
    GLuint oit_fbo, oit_accum_tex, oit_reveal_tex;

} renderer;

#define RENDER_OBJ_COUNT 3

static struct render_obj *gl_objs[] = {
    &renderer.volume_borders,
    &renderer.sources,
    &renderer.volume_pathtrace
};

static void init_camera() {
    struct camera *c = &renderer.camera;
    memset(c, 0, sizeof(*c));

    c->fov_y = 45 * (M_PI / 180);
    if (renderer.window.height > 0) {
        c->aspect = (float)renderer.window.width / renderer.window.height;
    }
    c->near_plane = 1.0f;
    c->far_plane = 100.0f;

    c->rot[0] = 1.0f;
    // c->pos[2] = -5.0f;

    camera_build_proj(c);
    camera_build_view(c);
}

static void window_resize_callback(GLFWwindow *window, int width, int height) {
    static struct window *w = &renderer.window;
    glViewport(0, 0, width, height);
    w->width = width;
    w->height = height;

    if (height > 0) {
        renderer.camera.aspect = (float)width / height;
        camera_build_proj(&renderer.camera);
    }
}

static void cursor_pos_callback(GLFWwindow *window, double x, double y) {
    static struct window *w = &renderer.window;
    w->curs_vx = x - w->curs_px;
    w->curs_vy = y - w->curs_py;
    w->curs_px = x;
    w->curs_py = y;
}

static int init_window(int width, int height, char *name) {
    //TODO: don't leak on failure.

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
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(w->ptr);
        glfwTerminate();
        return -1;
    }

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

#define VERTEX_VALUE_COUNT  3
#define BORDER_VERTEX_COUNT 8

extern float vertices[];

int init_renderer(simctx *ctx) {
    memset(&renderer, 0, sizeof(struct renderctx));

    if (!ctx) return -1;
    renderer.field = get_field(ctx);

    if (init_window(1400, 800, "floating") < 0) return -1;

    glGenTextures(1, &renderer.scene_depth_tex);
    glBindTexture(GL_TEXTURE_2D, renderer.scene_depth_tex);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_DEPTH_COMPONENT32F,
        renderer.window.width,
        renderer.window.height,
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        NULL
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    //glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    //
    //glFramebufferTexture2D(
    //    GL_FRAMEBUFFER,
    //    GL_DEPTH_ATTACHMENT,
    //    GL_TEXTURE_2D,
    //    depthTexture,
    //    0
    //);

    {  // Volume border
        struct render_obj *vb = &renderer.volume_borders;
        glGenBuffers(1, &vb->vbo);
        glGenBuffers(1, &vb->ebo);
        glGenVertexArrays(1, &vb->vao);

        glBindBuffer(GL_ARRAY_BUFFER, vb->vbo);
        glBindVertexArray(vb->vao);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vb->ebo);

        float hdSx = get_em_field_width(renderer.field) / 2;
        float hdSy = get_em_field_height(renderer.field) / 2;
        float hdSz = get_em_field_depth(renderer.field) / 2;

        // clang-format off
        float volume_border_vertices[] = {
            -hdSx, -hdSy, -hdSz,
             hdSx, -hdSy, -hdSz,
             hdSx,  hdSy, -hdSz,
            -hdSx,  hdSy, -hdSz,
            -hdSx, -hdSy,  hdSz,
             hdSx, -hdSy,  hdSz,
             hdSx,  hdSy,  hdSz,
            -hdSx,  hdSy,  hdSz,
        };
        // clang-format on
        glBufferData(GL_ARRAY_BUFFER, sizeof(volume_border_vertices), volume_border_vertices, GL_STATIC_DRAW);

        // clang-format off
        uint32_t volume_border_indices[] = {
            0, 4, 1, 5, 2, 6, 3, 7,
            0, 1, 1, 2, 2, 3, 3, 0,
            4, 5, 5, 6, 6, 7, 7, 4
        };
        // clang-format on

        vb->element_count = sizeof(volume_border_indices) / sizeof(volume_border_indices[0]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(volume_border_indices), volume_border_indices, GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        vb->shader = create_program("render/shaders/vs.glsl", "render/shaders/fs.glsl");

        struct border_uniform_locations *u = &renderer.border_uniforms;
        GLuint prog = vb->shader;
        glUseProgram(prog);
        u->proj = glGetUniformLocation(prog, "proj");
        u->view = glGetUniformLocation(prog, "view");
        glUseProgram(0);
    }

    {  // Sources
        struct render_obj *srcs = &renderer.sources;
        glGenBuffers(1, &srcs->vbo);
        srcs->ebo = 0;
        glGenVertexArrays(1, &srcs->vao);

        glBindBuffer(GL_ARRAY_BUFFER, srcs->vbo);
        glBindVertexArray(srcs->vao);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(0 * sizeof(float)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 288, vertices, GL_STATIC_DRAW);

        srcs->element_count = 288 / 8;

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        srcs->shader = create_program("render/shaders/source_vs.glsl", "render/shaders/source_fs.glsl");

        struct sources_uniform_locations *u = &renderer.sources_uniforms;
        GLuint prog = srcs->shader;
        glUseProgram(prog);
        u->proj = glGetUniformLocation(prog, "proj");
        u->view = glGetUniformLocation(prog, "view");
        u->model = glGetUniformLocation(prog, "model");
        u->light_angle = glGetUniformLocation(prog, "light_angle");
        glUseProgram(0);
    }

    {  // Volumetric Pathtracer
        // Screenspace quad for path tracing shader

        renderer.vol_step_size = 0.1;

        struct render_obj *vp = &renderer.volume_pathtrace;
        glGenBuffers(1, &vp->vbo);
        glGenVertexArrays(1, &vp->vao);

        glBindBuffer(GL_ARRAY_BUFFER, vp->vbo);
        glBindVertexArray(vp->vao);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        vp->ebo = 0;
        {
            float fullscreen_quad_vertices[] = {-1, -1, -1, 1, 1, 1, -1, -1, 1, -1, 1, 1};
            glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreen_quad_vertices), fullscreen_quad_vertices, GL_STATIC_DRAW);
            vp->element_count = sizeof(fullscreen_quad_vertices) / sizeof(fullscreen_quad_vertices[0]);
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
        GLuint prog = renderer.volume_pathtrace.shader = create_program("render/shaders/vol_vs.glsl", "render/shaders/vol_fs.glsl");
        glUseProgram(prog);
        glUniform1i(glGetUniformLocation(prog, "Etex"), 0);
        glUniform1i(glGetUniformLocation(prog, "Btex"), 1);
        glUniform3f(glGetUniformLocation(prog, "voxel_size"), renderer.field->dSx, renderer.field->dSy, renderer.field->dSz);
        glUniform1f(glGetUniformLocation(prog, "step_size"), renderer.vol_step_size);
        glUniform3f(
            glGetUniformLocation(prog, "dimensions"),
            get_em_field_width(renderer.field),
            get_em_field_height(renderer.field),
            get_em_field_depth(renderer.field)
        );
        // volumetric shader dynamic uniforms
        struct vol_uniform_locations *vds = &renderer.vol_uniforms;
        glUseProgram(prog);
        vds->camera_pos = glGetUniformLocation(prog, "camera_pos");
        vds->camera_forward = glGetUniformLocation(prog, "camera_forward");
        vds->camera_right = glGetUniformLocation(prog, "camera_right");
        vds->camera_up = glGetUniformLocation(prog, "camera_up");
        vds->intensity_E_field = glGetUniformLocation(prog, "intensity_E_field");
        vds->intensity_B_field = glGetUniformLocation(prog, "intensity_B_field");
        glUseProgram(0);
    }

    renderer.vol_magnitude_buffer = malloc(get_em_field_cell_count(renderer.field) * sizeof(float));

    glEnable(GL_DEPTH_TEST);

    init_camera();

    printf("GL init error: %x\n", glGetError());

    return 0;
}

void deinit_renderer() {
    for (int i = 0; i < RENDER_OBJ_COUNT; i++) {
        struct render_obj *obj = gl_objs[i];

        glDeleteBuffers(1, &obj->vbo);
        if (obj->ebo) glDeleteBuffers(1, &obj->ebo);
        glDeleteVertexArrays(1, &obj->vao);
        glDeleteProgram(obj->shader);
    }

    glDeleteTextures(1, &renderer.vol_Etex);
    glDeleteTextures(1, &renderer.vol_Btex);
    printf("GL Error: %x\n", glGetError());
    free(renderer.vol_magnitude_buffer);

    destroy_window();
}

static void buffer_components(float *restrict Fx, float *restrict Fy, float *restrict Fz, GLuint texture) {
    for (int i = 0; i < renderer.field->Nx; i++) {
        for (int j = 0; j < renderer.field->Ny; j++) {
            int idx = i * renderer.field->stride_x + j * renderer.field->stride_y;
            for (int k = 0; k < renderer.field->Nz; k++) {
                renderer.vol_magnitude_buffer[idx] = sqrtf(Fx[idx] * Fx[idx] + Fy[idx] * Fy[idx] + Fz[idx] * Fz[idx]);

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
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, renderer.field->Nz, renderer.field->Ny, renderer.field->Nx, GL_RED, GL_FLOAT, renderer.vol_magnitude_buffer);
}

void render_current() {
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_BACK);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Geometry
    // bind shader, accumulation, update uniforms, vao
    // draw geometry, collect depth and accumulation

    {  // Draw Volume Borders
        struct render_obj *vb = &renderer.volume_borders;
        struct border_uniform_locations *u = &renderer.border_uniforms;
        glUseProgram(vb->shader);
        glUniformMatrix4fv(u->proj, 1, GL_FALSE, renderer.camera.proj);
        glUniformMatrix4fv(u->view, 1, GL_FALSE, renderer.camera.view);
        glBindVertexArray(vb->vao);
        glDrawElements(GL_LINES, vb->element_count, GL_UNSIGNED_INT, NULL);
    }

    {  // Sources
        float axis[3] = {4, 4, -1};
        vec_normalize(axis, axis, 3);
        float angle = 0.3 * M_PI;
        float s = sin(angle);
        float rotation_axis[4] = {cos(angle), axis[0] * s, axis[1] * s, axis[2] * s};

        float container_rotator[16];
        mat4_from_quat(container_rotator, rotation_axis);

        struct render_obj *srcs = &renderer.sources;
        struct sources_uniform_locations *u = &renderer.sources_uniforms;
        glBindVertexArray(srcs->vao);
        glUseProgram(srcs->shader);
        glUniformMatrix4fv(u->proj, 1, GL_FALSE, renderer.camera.proj);
        glUniformMatrix4fv(u->view, 1, GL_FALSE, renderer.camera.view);
        glUniform3f(u->light_angle, -0.0f, 1.0f, -0.0f);

        float model[16];
        mat_identity(model, 4);

        for (int i = 0; i < N_SOURCES; i++) {
            mat_mul(model, container_rotator, model, 4, 4, 4);
            model[0 + 3 * 4] = source_positions[i][0];
            model[1 + 3 * 4] = source_positions[i][1];
            model[2 + 3 * 4] = source_positions[i][2];

            glUniformMatrix4fv(u->model, 1, GL_FALSE, model);
            glDrawArrays(GL_TRIANGLES, 0, srcs->element_count);
        }
    }

    // Volumme
    // bind shader, accumulation, depth, textures, update uniforms, vao
    // render volume over geometry

    glfwSwapBuffers(renderer.window.ptr);
    return;
}

#define CAMERA_SPEED_HORIZONTAL 0.2
#define CAMERA_SPEED_VERTICAL   0.1

void process_input() {
    // TODO: Rewrite everything

    glfwPollEvents();

    struct window *w = &renderer.window;
    struct camera *c = &renderer.camera;

    // Arrow Key camera pivot

    // q_up(_inv) rotates by +/- 2*h_angle around worldspace up (y) vector.
    float h_angle = 0.02;
    float sin = sinf(h_angle), cos = cosf(h_angle);
    float q_up[4] = {cos, 0.0f, sin, 0.0f};
    float q_up_inv[4];
    quat_conj(q_up_inv, q_up);

    // q_left(_inv) rotates by +/- 2*h_angle around the camera-space left (x) vector.
    float q_left[4] = {0.0f, 1.0f, 0.0f, 0.0f};
    quat_sandwitch(q_left, q_left, c->rot);
    q_left[0] = cos;
    q_left[1] *= sin;
    q_left[2] *= sin;
    q_left[3] *= sin;
    float q_left_inv[4];
    quat_conj(q_left_inv, q_left);

    if (glfwGetKey(w->ptr, GLFW_KEY_LEFT) == GLFW_PRESS) {
        quat_mul(c->rot, q_up, c->rot);
    }
    if (glfwGetKey(w->ptr, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        quat_mul(c->rot, q_up_inv, c->rot);
    }

    // lock rotation to straight vertical and horizontal

    // camera-space forward, needed for angle lock and W/S movement later
    float q_forward[4] = {0, 0, 0, -1};
    quat_sandwitch(q_forward, q_forward, c->rot);
    float *camera_forward = q_forward + 1;

    float dot = vec_dot(camera_forward, (float[3]){0, 1.0, 0}, 3);
    float dot_limit = vec_length(camera_forward, 3) - 0.1 * (M_PI / 180);  // length of camera_forward minus 0.1° marigin

    if (dot < dot_limit && glfwGetKey(w->ptr, GLFW_KEY_UP) == GLFW_PRESS) {
        quat_mul(c->rot, q_left, c->rot);
    }

    if (dot > -dot_limit && glfwGetKey(w->ptr, GLFW_KEY_DOWN) == GLFW_PRESS) {
        quat_mul(c->rot, q_left_inv, c->rot);
    }

    // WASD/Space/Shift movement

    if (glfwGetKey(w->ptr, GLFW_KEY_SPACE) == GLFW_PRESS) {
        c->pos[1] += CAMERA_SPEED_VERTICAL;
    }
    if (glfwGetKey(w->ptr, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        c->pos[1] -= CAMERA_SPEED_VERTICAL;
    }

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

// clang-format off
float vertices[288] = {
    -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
    0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,
    0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
    0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
    -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,

    -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
    0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
    0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,

    -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,

    0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
    0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,

    -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,
    0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,
    0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,

    -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
    0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
    0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
    -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
// clang-format on
