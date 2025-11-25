#include "external/glad/glad.h"
#include "maxwell-solver.h"

#include <assert.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
        void* buffer;
        long  size;
} file;

file load_file(const char* path);
void free_file(file);

#define VS_PATH "shaders/vs_basic.glsl"
#define FS_PATH "shaders/fs_basic.glsl"

int         frame_width = 0, frame_height = 0;
GLFWwindow* window_ptr = NULL;
float       aspect     = 0;

GLuint triangle_vao = 0;

GLuint             basic_shader_prog   = 0;
GLuint             u_transform         = 0;
GLuint             u_window_dimensions = 0;
// clang-format off
const float triangle_vertices[] = {
          // back face
              -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
              -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
              -0.5f,  0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
              -0.5f, -0.5f, 0.5f, 1.0f, 1.0f, 0.0f,
          // front face
               0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.0f,
               0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
               0.5f,  0.5f, 0.5f, 0.0f, 1.0f, 0.5f,
               0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 1.0f,
};
// clang-format on
const unsigned int cube_indices[]      = { 0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
                                           0, 1, 5, 5, 4, 0, 3, 2, 6, 6, 7, 3,
                                           0, 3, 7, 7, 4, 0, 1, 2, 6, 6, 5, 1 };

typedef enum { VERTEX, FRAGMENT } shader_type;

uint32_t compile_shader_from_path(const char* path, shader_type type);

void framebuffer_size_callback(GLFWwindow* window, int w, int h) {
        int x_border = (w - frame_width) / 2;
        int y_border = (h - frame_height) / 2;

        if (x_border < 0) {
                x_border = 0;
        }
        if (y_border < 0) {
                y_border = 0;
        }

        glViewport(x_border, y_border, frame_width, frame_height);
}

void init_resources();

void start_renderer(int width, int height) {
        frame_width  = width;
        frame_height = height;
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window_ptr = glfwCreateWindow(width, height, "floating", NULL, NULL);

        if (window_ptr == NULL) {
                glfwTerminate();
                fprintf(stderr, "Failed to create GLFW window");
        }
        glfwMakeContextCurrent(window_ptr);

        if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
                fprintf(stderr, "Failed to initialize GLAD");
        }
        glViewport(0, 0, frame_width, frame_height);
        glfwSetFramebufferSizeCallback(window_ptr, framebuffer_size_callback);

        init_resources();
        u_transform = glGetUniformLocation(basic_shader_prog, "transform");
        u_window_dimensions =
                glGetUniformLocation(basic_shader_prog, "window_dimensions");
        glUniform2f(u_window_dimensions, frame_width, frame_height);
        aspect = (float) frame_width / (float) frame_height;

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glEnable(GL_DEPTH_TEST);
}

void draw() {
        if (glfwGetKey(window_ptr, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window_ptr, 1);
        };

        static float transform[16];
        float        time  = (float) glfwGetTime();
        float        angle = time;  // radians, rotates at 1 rad/s
        float        c     = cosf(angle);
        float        s     = sinf(angle);

        // Row-major rotation around Y axis
        transform[0]  = c;
        transform[1]  = 0;
        transform[2]  = s;
        transform[3]  = 0;
        transform[4]  = 0;
        transform[5]  = 1;
        transform[6]  = 0;
        transform[7]  = 0;
        transform[8]  = -s;
        transform[9]  = 0;
        transform[10] = c;
        transform[11] = 0;
        transform[12] = 0;
        transform[13] = 0;
        transform[14] = 0;
        transform[15] = 1;

        glUniformMatrix4fv(u_transform, 1, GL_FALSE, transform);
        glBindVertexArray(triangle_vao);
        glUseProgram(basic_shader_prog);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glfwSwapBuffers(window_ptr);
}

void quit_renderer() {
        glfwDestroyWindow(window_ptr);
        glfwTerminate();
}

void init_resources() {
        glGenVertexArrays(1, &triangle_vao);
        glBindVertexArray(triangle_vao);

        GLuint cube_vbo = 0;
        glGenBuffers(1, &cube_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_vertices),
                     triangle_vertices, GL_STATIC_DRAW);

        GLuint cube_ebo;
        glGenBuffers(1, &cube_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices),
                     cube_indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                              (void*) 0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                              (void*) (3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        GLuint vs_basic   = compile_shader_from_path(VS_PATH, VERTEX);
        GLuint fs_basic   = compile_shader_from_path(FS_PATH, FRAGMENT);
        basic_shader_prog = glCreateProgram();
        glAttachShader(basic_shader_prog, vs_basic);
        glAttachShader(basic_shader_prog, fs_basic);
        glLinkProgram(basic_shader_prog);
        glDeleteShader(vs_basic);
        glDeleteShader(fs_basic);
}

int should_close() {
        glfwPollEvents();
        return glfwWindowShouldClose(window_ptr);
}

uint32_t compile_shader_from_path(const char* path, shader_type type) {
        file src_file = load_file(path);
        if (src_file.size < 1) {
                printf("Error loading shader from file \"");
                printf("%s", path);
                printf("\"\n");
                free_file(src_file);
                return 0;
        }

        GLuint shader = 0;
        if (type == VERTEX) {
                shader = glCreateShader(GL_VERTEX_SHADER);
        } else if (type == FRAGMENT) {
                shader = glCreateShader(GL_FRAGMENT_SHADER);
        } else {
                return 0;
        }

        const char* srcv[] = { src_file.buffer };
        GLint length[]     = { (GLint) src_file.size };  // no null termination
        glShaderSource(shader, 1, srcv, length);
        free_file(src_file);

        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success == GL_TRUE) {
                return shader;
        }

        int len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        char* log = malloc(len + 1);
        glGetShaderInfoLog(shader, len, NULL, log);
        log[len] = '\n';
        printf("Shader loaded from \"");
        printf("%s", path);
        printf("\" failed to compile:\n    ");
        printf("%s\n", log);
        free(log);
        return 0;
}

file load_file(const char* path) {
        FILE* fp = fopen(path, "rb");
        if (!fp) {
                return (file) { NULL, 0 };
        }

        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (size < 0) {
                fclose(fp);
                return (file) { NULL, 0 };
        }
        void* buffer = malloc(size);
        if (!buffer) {
                fclose(fp);
                return (file) { NULL, 0 };
        }

        fread(buffer, 1, size, fp);
        fclose(fp);

        return (file) { buffer, size };
}

void free_file(file f) {
        free(f.buffer);
}
