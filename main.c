#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define GL_ERR()                                           \
    {                                                      \
        GLenum err = GL_NO_ERROR;                          \
        while ((err = glGetError()) != GL_NO_ERROR) {      \
            printf("error %s\n", glewGetErrorString(err)); \
            printf("%d\n", err);                           \
        }                                                  \
    }

void die(char* msg) {
    printf("%s\n", msg);
    exit(1);
}

const char* vertex_shader_source = R"GLSL(
    #version 450 core
    layout (location = 0) in vec3 pos;
    layout (location = 1) in vec2 tex_coord_attrib;

    out vec2 tex_coord;
    void main() {

        gl_Position = vec4(pos, 1.0);
        tex_coord = tex_coord_attrib;

    }
)GLSL";
#if defined(BIND)
const char* fragment_shader_source = R"GLSL(
    #version 450 core

    in vec2 tex_coord;

    layout (binding = 0) uniform sampler2D tex;
    out vec4 FragColor;

    void main() {
        FragColor = texture(tex, tex_coord);
    }
)GLSL";
#else
const char* fragment_shader_source = R"GLSL(
    #version 450 core
    #extension GL_ARB_bindless_texture : enable

    in vec2 tex_coord;

    layout(bindless_sampler) uniform sampler2D tex;
    out vec4 FragColor;

    void main() {
        FragColor = texture(tex, tex_coord);
    }
)GLSL";
#endif

const char* compute_shader_source = R"GLSL(
    #version 450 core

    layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
    layout(rgba8, binding = 0) uniform writeonly image2D target;

    void main() {
        ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
        vec2 shift_coords = coords - 50;
        float dist = sqrt(shift_coords.x*shift_coords.x+shift_coords.y*shift_coords.y)/sqrt(50*50);
        imageStore(target, coords, vec4(dist, .0, .0, 1.));
    }
)GLSL";

float vertices[] = {
    -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, 1.0f,  1.0f, 0.0f, 1.0f,
    1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 1.0f, 0.0f,
};

unsigned int indices[] = {0, 1, 2, 0, 2, 3};

GLFWwindow* create_window() {
    GLFWwindow* gl_ctx;
    GLenum err;

    glfwInit();
    glfwDefaultWindowHints();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    gl_ctx = glfwCreateWindow(100, 100, "Display OpenGL texture", NULL, NULL);
    if (!gl_ctx) {
        die("Failed to create opengl context");
    }

    glfwMakeContextCurrent(gl_ctx);

    glewExperimental = 1;
    err = glewInit();
    if (GLEW_OK != err) {
        die("Failed to init glew");
    }

    return gl_ctx;
}

GLuint create_texture() {
    GLuint texture;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 100, 100, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    unsigned char foo[4] = {249, 203, 67, 255};
    glClearTexImage(texture, 0, GL_RGBA, GL_UNSIGNED_BYTE, foo);

    return texture;
}

void compute_texture(GLuint texture) {
    GLint is_compiled, is_linked;
    GLuint shader, program;
    shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &compute_shader_source, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        char error_buf[maxLength];
        glGetShaderInfoLog(shader, maxLength, &maxLength, &error_buf[0]);
        die(error_buf);
    }

    program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);

    glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
    if (is_linked == GL_FALSE) {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
        char error_buf[maxLength];
        glGetProgramInfoLog(program, maxLength, &maxLength, &error_buf[0]);
        die(error_buf);
    }

    glUseProgram(program);
    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glDispatchCompute(100, 100, 1);  // YEAH not good but this is just a bug report
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

#if defined(HACK)
    glBindTexture(GL_TEXTURE_2D, texture);
    glActiveTexture(GL_TEXTURE0);
    glGenerateMipmap(GL_TEXTURE_2D);
#endif

    glUseProgram(0);
}

void draw_texture(GLuint texture) {
    GLuint program, vao, vbo, ebo, vertex_shader, fragment_shader;
    int success;
    char info_log[512];

#if defined(BINDLESS)
    GLint fs_loc_tex;
#endif

    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);

    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
        fprintf(stderr, "Failed to compile vertex shader: %s\n", info_log);
    }

    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
        fprintf(stderr, "Failed to compile fragment shader: %s\n", info_log);
    }

    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

#if defined(BINDLESS)
    fs_loc_tex = glGetUniformLocation(program, "tex");
#endif

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glUseProgram(program);

#if defined(BIND)
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, texture);
#endif
#if defined(BINDLESS)
    GLuint64 texture_handle = glGetTextureHandleARB(texture);
    if (!glIsTextureHandleResidentARB(texture_handle)) {
        glMakeTextureHandleResidentARB(texture_handle);
    }
    glProgramUniformHandleui64ARB(program, fs_loc_tex, texture_handle);
#endif

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    GLFWwindow* window = create_window();
    GLuint texture = create_texture();

    compute_texture(texture);
    draw_texture(texture);

    glfwSwapBuffers(window);
    glFlush();
    GL_ERR();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        usleep(1.0f / 30 * 1000000.0f);
    }

    // CLEANUP OMITTED
    return EXIT_SUCCESS;
}
