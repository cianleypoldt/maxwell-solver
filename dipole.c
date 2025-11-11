#include "external/glad/glad.h"
#include "maxwell-solver.c"

#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

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

GLuint compileShader(GLenum type, const char *source) {
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

GLuint createShaderProgram(const char *vertexSrc, const char *fragmentSrc) {
    GLuint vertexShader   = compileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);

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

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main() {
    init_simulation(1, 1, 1, 50);

    glfwInit();
    GLFWwindow *window = glfwCreateWindow(800, 600, "floating", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        return -1;
    }

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);

    char *vs_shader = readFile("shaders/vs_render_quad.glsl");
    char *fs_shader = readFile("shaders/fs_gpt.glsl");

    GLuint shader_program = createShaderProgram(vs_shader, fs_shader);

    free(vs_shader);
    free(fs_shader);

    glUseProgram(shader_program);

    GLint iTimeLocation       = glGetUniformLocation(shader_program, "iTime");
    GLint iResolutionLocation = glGetUniformLocation(shader_program, "iResolution");

    while (!glfwWindowShouldClose(window) && !(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)) {
        float time = (float) glfwGetTime();
        int   width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glUniform1f(iTimeLocation, time);
        glUniform2f(iResolutionLocation, (float) width, (float) height);

        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(shader_program);
    glfwDestroyWindow(window);
    glfwTerminate();

    destroy_simulation();
}
