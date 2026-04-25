#include "renderer.h"
#include "volume_loader.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <fstream>
#include <sstream>
#include <iostream>

// =====================================
// SHADER UTILS
// =====================================
static std::string loadFile(const char* path) {
    std::ifstream file(path);
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static unsigned int compileShader(const char* path, GLenum type) {
    std::string src = loadFile(path);
    const char* c = src.c_str();

    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &c, nullptr);
    glCompileShader(shader);

    return shader;
}

static unsigned int createProgram(const char* vs, const char* fs) {
    unsigned int v = compileShader(vs, GL_VERTEX_SHADER);
    unsigned int f = compileShader(fs, GL_FRAGMENT_SHADER);

    unsigned int p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);

    glDeleteShader(v);
    glDeleteShader(f);

    return p;
}

// =====================================
// CONSTRUCTOR
// =====================================
Renderer::Renderer(int w, int h)
    : width(w), height(h)
{
    initCube();
    initShaders();
    initVolume();
}

// =====================================
// VOLUME → GPU
// =====================================
void Renderer::initVolume() {
    Volume vol = loadVolumeAuto("../data/sub-G2106_ses-01_T1w.nii");

    glGenTextures(1, &volumeTex);
    glBindTexture(GL_TEXTURE_3D, volumeTex);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glTexImage3D(
        GL_TEXTURE_3D,
        0,
        GL_RED,
        vol.width,
        vol.height,
        vol.depth,
        0,
        GL_RED,
        GL_FLOAT,
        vol.data.data()
    );

    std::cout << "Volume loaded: "
              << vol.width << "x"
              << vol.height << "x"
              << vol.depth << std::endl;
}

// =====================================
// CUBE (BOUNDING BOX)
// =====================================
void Renderer::initCube() {
    float vertices[] = {
        0,0,0,  1,0,0,  1,1,0,  0,1,0,
        0,0,1,  1,0,1,  1,1,1,  0,1,1
    };

    unsigned int indices[] = {
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };

    glGenVertexArrays(1, &cubeVAO);
    glBindVertexArray(cubeVAO);

    glGenBuffers(1, &cubeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &cubeEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

// =====================================
// SHADERS
// =====================================
void Renderer::initShaders() {
    wireProgram = createProgram(
        "shaders/wire.vert",
        "shaders/wire.frag"
    );

    volumeProgram = createProgram(
        "shaders/volume.vert",
        "shaders/volume.frag"
    );
}

// =====================================
// CAMERA
// =====================================
void Renderer::onMouseDrag(float dx, float dy) {
    float s = 0.005f;
    yaw += dx * s;
    pitch += dy * s;

    if (pitch > 1.5f) pitch = 1.5f;
    if (pitch < -1.5f) pitch = -1.5f;
}

void Renderer::onZoom(float delta) {
    distance -= delta * 0.2f;

    if (distance < 0.5f) distance = 0.5f;
    if (distance > 10.0f) distance = 10.0f;
}

glm::mat4 Renderer::getView() {
    glm::vec3 c(0.5f);

    glm::vec3 p;
    p.x = c.x + distance * cos(pitch) * sin(yaw);
    p.y = c.y + distance * sin(pitch);
    p.z = c.z + distance * cos(pitch) * cos(yaw);

    return glm::lookAt(p, c, glm::vec3(0,1,0));
}

glm::mat4 Renderer::getProj() {
    int w, h;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &w, &h);

    return glm::perspective(glm::radians(45.0f),
                            (float)w/h,
                            0.1f,
                            100.0f);
}

// =====================================
// RENDER
// =====================================
void Renderer::render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 mvp = getProj() * getView();

    // -----------------------------
    // 1. DRAW VOLUME (SIMPLE)
    // -----------------------------
    glUseProgram(volumeProgram);

    glUniformMatrix4fv(
        glGetUniformLocation(volumeProgram, "mvp"),
        1, GL_FALSE, &mvp[0][0]
    );

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, volumeTex);

    glUniform1i(
        glGetUniformLocation(volumeProgram, "volumeTex"),
        0
    );

    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0); // volume proxy

    // -----------------------------
    // 2. DRAW WIREFRAME
    // -----------------------------
    glUseProgram(wireProgram);

    glUniformMatrix4fv(
        glGetUniformLocation(wireProgram, "mvp"),
        1, GL_FALSE, &mvp[0][0]
    );

    glBindVertexArray(cubeVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
}