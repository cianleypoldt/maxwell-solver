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

struct frame_data {
    float proj[16];
    float view[16];
    float view_proj[16];
    float camera_pos[3];
    float time;
};

struct mesh {
    GLuint vbo, ebo, vao;
    int index_count;
};

enum mesh_layout {
    POS,
    POS_NORM,
};

static void mesh_create(struct mesh *m, float *vertices, int vcount, uint32_t *indices, int icount, enum mesh_layout layout, int vertex_value_count) {
    glGenBuffers(1, &m->vbo);
    glGenBuffers(1, &m->ebo);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vcount * vertex_value_count, vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * icount, indices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &m->vao);
    glBindVertexArray(m->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo);  // stored by VAO

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_value_count * sizeof(float), (void *)(0 * sizeof(float)));
    glEnableVertexAttribArray(0);
    if (layout == POS_NORM) {
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_value_count * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);  // no effect on VAO state
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m->index_count = icount;
}

static void mesh_destroy(struct mesh *m) {
    glDeleteBuffers(1, &m->vbo);
    glDeleteBuffers(1, &m->ebo);
    glDeleteVertexArrays(1, &m->vao);
    memset(m, 0, sizeof(*m));
}

inline static void mesh_draw(const struct mesh m, GLenum mode) {
    glBindVertexArray(m.vao);
    glDrawElements(mode, m.index_count, GL_UNSIGNED_INT, NULL);
}

#define N_SOURCES 10
const float source_positions[N_SOURCES][3] = {
    {12.0f, 12.0f, 12.0f},
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

    GLuint frame_data_ubo;
    struct frame_data frame_data;

    // Shared
    GLuint scene_fbo, scene_color_tex, scene_depth_tex;

    // Borders, probably temp
    struct mesh fullscreen_quad;
    struct mesh unit_cube;
    struct mesh unit_cube_wireframe;
    GLuint shader_sources;
    GLint uloc_wf_model;

    // Sources, probably temp & to be included in general opaque abstraction
    GLuint shader_wireframe;
    GLint uloc_srcs_model, uloc_srcs_light_angle;

    // Opaque geometry abstraction:
    //   - requires a stronger shader and uniform abstraction
    //   - Mesh + shader + init (defaults or custom) + render (d.o.c.) + destroy (default as well as custom)

    // transparent geometry (WOIT), yet to be implemented
    // struct render_obj oit_geometry;
} renderer;

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

// clang-format off
static float QUAD_VERTS[4*3] = {
    -1.0f, -1.0f, 0,
     1.0f, -1.0f, 0,
    -1.0f,  1.0f, 0,
     1.0f,  1.0f, 0
};

static uint32_t QUAD_INDICES[6] = {
    0, 2, 3, 0, 3, 1
};

static float CUBE_VERTS[8 * 3] = {
	-0.5f, -0.5f, -0.5f, // 0: (-,-,-)
	0.5f,  -0.5f, -0.5f, // 1: (+,-,-)
	-0.5f, 0.5f,  -0.5f, // 2: (-,+,-)
	0.5f,  0.5f,  -0.5f, // 3: (+,+,-)
	-0.5f, -0.5f, 0.5f,	 // 4: (-,-,+)
	0.5f,  -0.5f, 0.5f,	 // 5: (+,-,+)
	-0.5f, 0.5f,  0.5f,	 // 6: (-,+,+)
	0.5f,  0.5f,  0.5f,	 // 7: (+,+,+)
};
static uint32_t CUBE_INDICES[36] = {
	0, 4, 6, 0, 6, 2, // -X
	1, 3, 7, 1, 7, 5, // +X
	0, 1, 5, 0, 5, 4, // -Y
	2, 6, 7, 2, 7, 3, // +Y
	0, 2, 3, 0, 3, 1, // -Z
	4, 5, 7, 4, 7, 6, // +Z
};

static uint32_t WIREFRAME_CUBE_INDICES[36] = {
    0, 1, 1, 3, 3, 2, 2, 0,        // z = -0.5 face
    4, 5, 5, 7, 7, 6, 6, 4,    // z = +0.5 face
    0, 4, 1, 5, 2, 6, 3, 7,// connecting edges
};
// clang-format on

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

int init_renderer(simctx *ctx) {
    memset(&renderer, 0, sizeof(struct renderctx));

    if (!ctx) return -1;
    renderer.field = get_field(ctx);

    if (init_window(1400, 800, "floating") < 0) return -1;
    init_camera();

    // Perframe data UBO
    glGenBuffers(1, &renderer.frame_data_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, renderer.frame_data_ubo);
    glBufferData(
        GL_UNIFORM_BUFFER,
        sizeof(struct frame_data),
        NULL,
        GL_DYNAMIC_DRAW
    );
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, renderer.frame_data_ubo);  // Bind frame_data_ubo to binding point 0
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Meshes
    mesh_create(&renderer.fullscreen_quad, QUAD_VERTS, ARRAY_COUNT(QUAD_VERTS), QUAD_INDICES, ARRAY_COUNT(QUAD_INDICES), POS, 3);
    mesh_create(&renderer.unit_cube, CUBE_VERTS, ARRAY_COUNT(CUBE_VERTS), CUBE_INDICES, ARRAY_COUNT(CUBE_INDICES), POS, 3);
    mesh_create(&renderer.unit_cube_wireframe, CUBE_VERTS, ARRAY_COUNT(CUBE_VERTS), WIREFRAME_CUBE_INDICES, ARRAY_COUNT(WIREFRAME_CUBE_INDICES), POS, 3);

    // Shaders
    renderer.shader_wireframe = create_program("render/shaders/orange_vs.glsl", "render/shaders/orange_fs.glsl");
    renderer.shader_sources = create_program("render/shaders/source_vs.glsl", "render/shaders/source_fs.glsl");

    {  // wireframe uniform locatios
        float model[16] = {0};
        model[MAT_IDX(0, 0, 4, 4)] = get_em_field_width(renderer.field);
        model[MAT_IDX(1, 1, 4, 4)] = get_em_field_height(renderer.field);
        model[MAT_IDX(2, 2, 4, 4)] = get_em_field_depth(renderer.field);
        model[MAT_IDX(3, 3, 4, 4)] = 1;
        mat_print(model, 4, 4);

        GLuint pwireframe = renderer.shader_wireframe;
        glUseProgram(pwireframe);
        glUniformMatrix4fv(glGetUniformLocation(pwireframe, "model"), 1, GL_FALSE, model);
    }

    // sources uniform locations
    GLuint psources = renderer.shader_sources;
    glUseProgram(psources);
    renderer.uloc_srcs_model = glGetUniformLocation(psources, "model");
    renderer.uloc_srcs_light_angle = glGetUniformLocation(psources, "light_angle");
    glUseProgram(0);

    glEnable(GL_DEPTH_TEST);
    printf("GL init error: %x\n", glGetError());
    return 0;
}

void deinit_renderer() {
    mesh_destroy(&renderer.fullscreen_quad);
    mesh_destroy(&renderer.unit_cube);
    mesh_destroy(&renderer.unit_cube_wireframe);

    glDeleteProgram(renderer.shader_sources);
    glDeleteProgram(renderer.shader_wireframe);

    printf("GL Error: %x\n", glGetError());
    destroy_window();
}

void render_current() {
    {  // update per frame uniform data
        struct frame_data frame_data = {0};
        struct camera *c = &renderer.camera;
        memcpy(frame_data.proj, c->proj, 16 * sizeof(float));
        memcpy(frame_data.view, c->view, 16 * sizeof(float));
        mat_mul(frame_data.view_proj, c->proj, c->view, 4, 4, 4);
        memcpy(frame_data.camera_pos, c->pos, 3 * sizeof(float));

        glBindBuffer(GL_UNIFORM_BUFFER, renderer.frame_data_ubo);
        glBufferSubData(
            GL_UNIFORM_BUFFER,
            0,
            sizeof(struct frame_data),
            &frame_data
        );
    }

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Geometry
    // bind shader, accumulation, update uniforms, vao
    // draw geometry, collect depth and accumulation

    {  // Draw Volume Borders

        glUseProgram(renderer.shader_wireframe);
        mesh_draw(renderer.unit_cube_wireframe, GL_LINES);
    }

    {  // Sources
        float axis[3] = {4, 4, -1};
        vec_normalize(axis, axis, 3);
        float angle = 0.3 * M_PI;
        float s = sin(angle);
        float rotation_axis[4] = {cos(angle), axis[0] * s, axis[1] * s, axis[2] * s};

        float container_rotator[16];
        mat4_from_quat(container_rotator, rotation_axis);

        glUseProgram(renderer.shader_sources);
        glUniform3f(renderer.uloc_srcs_light_angle, -0.0f, 1.0f, -0.0f);

        float model[16];
        mat_identity(model, 4);

        for (int i = 0; i < N_SOURCES; i++) {
            mat_mul(model, container_rotator, model, 4, 4, 4);
            model[0 + 3 * 4] = source_positions[i][0];
            model[1 + 3 * 4] = source_positions[i][1];
            model[2 + 3 * 4] = source_positions[i][2];

            glUniformMatrix4fv(renderer.uloc_srcs_model, 1, GL_FALSE, model);
            mesh_draw(renderer.unit_cube, GL_TRIANGLES);
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
