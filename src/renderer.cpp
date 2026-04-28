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

    // Debug: check compilation
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cout << "[SHADER ERROR] " << path << ": " << infoLog << std::endl;
    }

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
    // IMPORTANTE: initVolume() primeiro para ter as dimensões
    initVolume();
    initCube();
    initShaders();
}

// =====================================
// VOLUME → GPU
// =====================================
void Renderer::initVolume()
{
    std::string rawPath = "data/ct.raw";
    std::string metaPath = "data/ct.txt";

    int W, H, D;

    std::ifstream meta(metaPath);
    if (!meta)
        throw std::runtime_error("Missing ct.txt (run Python first)");

    meta >> W >> H >> D;

    Volume vol = loadRAW(rawPath, W, H, D);

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
        GL_R32F,
        GL_R32F,
        vol.width,
        vol.height,
        vol.depth,
        0,
        GL_RED,
        GL_FLOAT,
        vol.data.data()
    );

    volumeWidth = vol.width;
    volumeHeight = vol.height;
    volumeDepth = vol.depth;
}

// =====================================
// CUBE (BOUNDING BOX) - adapta-se às dimensões do volume
// =====================================
void Renderer::initCube() {
    // Usar as dimensões reais do volume (normalizado para que o maior eixo = 1)
    float maxDim = static_cast<float>(
        volumeWidth > volumeHeight ? volumeWidth : volumeHeight
    );
    maxDim = maxDim > volumeDepth ? maxDim : static_cast<float>(volumeDepth);

    float scaleX = static_cast<float>(volumeWidth) / maxDim;
    float scaleY = static_cast<float>(volumeHeight) / maxDim;
    float scaleZ = static_cast<float>(volumeDepth) / maxDim;

    float vertices[] = {
        0,0,0,     scaleX,0,0,     scaleX,scaleY,0,  0,scaleY,0,
        0,0,scaleZ,  scaleX,0,scaleZ,  scaleX,scaleY,scaleZ,  0,scaleY,scaleZ
    };

    unsigned int indices[] = {
        // front
        0,1,2, 2,3,0,
        // back
        4,5,6, 6,7,4,
        // left
        0,3,7, 7,4,0,
        // right
        1,5,6, 6,2,1,
        // bottom
        0,1,5, 5,4,0,
        // top
        3,2,6, 6,7,3
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

    raycastProgram = createProgram(
        "shaders/raycast.vert",
        "shaders/raycast.frag"
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

void Renderer::toggleDebug() {
    debugEnabled = !debugEnabled;
    std::cout << "[DEBUG] " << (debugEnabled ? "ON" : "OFF") << std::endl;
}

glm::mat4 Renderer::getView() {
    // Calcular escala baseada nas dimensões do volume
    float maxDim = static_cast<float>(
        volumeWidth > volumeHeight ? volumeWidth : volumeHeight
    );
    maxDim = maxDim > volumeDepth ? maxDim : static_cast<float>(volumeDepth);

    float scaleX = static_cast<float>(volumeWidth) / maxDim;
    float scaleY = static_cast<float>(volumeHeight) / maxDim;
    float scaleZ = static_cast<float>(volumeDepth) / maxDim;

    // Centro do cubo = meio das dimensões
    glm::vec3 c(scaleX / 2.0f, scaleY / 2.0f, scaleZ / 2.0f);

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

void Renderer::adjustThreshold(float v)
{
    threshold += v;
    if (threshold < 0.0f) threshold = 0.0f;
    if (threshold > 1.0f) threshold = 1.0f;
}

void Renderer::adjustDensity(float v)
{
    density += v;
    if (density < 0.0f) density = 0.0f;
    if (density > 1.0f) density = 1.0f;
}

void Renderer::adjustBrightness(float v)
{
    brightness += v;
    if (brightness < 0.1f) brightness = 0.1f;
    if (brightness > 5.0f) brightness = 5.0f;
}

// =====================================
// RENDER
// =====================================
void Renderer::render() {
    // Debug output (toggle com 'd')
    if (debugEnabled) {
        std::cout << "=== DEBUG ===" << std::endl;
        std::cout << "Volume: " << volumeWidth << "x" << volumeHeight << "x" << volumeDepth << std::endl;
        
        float maxDim = static_cast<float>(
            volumeWidth > volumeHeight ? volumeWidth : volumeHeight
        );
        maxDim = maxDim > volumeDepth ? maxDim : static_cast<float>(volumeDepth);
        float scaleX = static_cast<float>(volumeWidth) / maxDim;
        float scaleY = static_cast<float>(volumeHeight) / maxDim;
        float scaleZ = static_cast<float>(volumeDepth) / maxDim;
        
        std::cout << "Cube scale: " << scaleX << "x" << scaleY << "x" << scaleZ << std::endl;
        std::cout << "Camera: dist=" << distance << ", yaw=" << yaw << ", pitch=" << pitch << std::endl;
        std::cout << "Center: " << (scaleX/2.0f) << ", " << (scaleY/2.0f) << ", " << (scaleZ/2.0f) << std::endl;
        std::cout << "=============" << std::endl;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Calcular matrizes
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = getView();
    glm::mat4 proj = getProj();
    glm::mat4 mvp = proj * view * model;

    // Calcular escala para centered do cubo
    float maxDim = static_cast<float>(
        volumeWidth > volumeHeight ? volumeWidth : volumeHeight
    );
    maxDim = maxDim > volumeDepth ? maxDim : static_cast<float>(volumeDepth);
    float scaleX = static_cast<float>(volumeWidth) / maxDim;
    float scaleY = static_cast<float>(volumeHeight) / maxDim;
    float scaleZ = static_cast<float>(volumeDepth) / maxDim;

    // -----------------------------
    // 1. DRAW VOLUME (RAYCAST)
    // -----------------------------
    glUseProgram(raycastProgram);

    glUniformMatrix4fv(
        glGetUniformLocation(raycastProgram, "model"),
        1, GL_FALSE, &model[0][0]
    );
    glUniformMatrix4fv(
        glGetUniformLocation(raycastProgram, "view"),
        1, GL_FALSE, &view[0][0]
    );
    glUniformMatrix4fv(
        glGetUniformLocation(raycastProgram, "proj"),
        1, GL_FALSE, &proj[0][0]
    );

    // Pass volume scale to shader
    glUniform3f(
        glGetUniformLocation(raycastProgram, "volumeScale"),
        scaleX,
        scaleY,
        scaleZ
    );

    glUniform1f(glGetUniformLocation(raycastProgram, "uThreshold"), threshold);
    glUniform1f(glGetUniformLocation(raycastProgram, "uDensity"), density);
    glUniform1f(glGetUniformLocation(raycastProgram, "uBrightness"), brightness);

    glm::vec3 camPos = glm::inverse(view)[3];
        glUniform3f(
        glGetUniformLocation(raycastProgram, "cameraPos"),
        camPos.x, camPos.y, camPos.z
    );

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, volumeTex);

    glUniform1i(
        glGetUniformLocation(raycastProgram, "volumeTex"),
        0
    );

    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0); // volume proxy

    // -----------------------------
    // 2. DRAW WIREFRAME
    // -----------------------------
    glDisable(GL_DEPTH_TEST);

    glDisable(GL_DEPTH_TEST);

    glUseProgram(wireProgram);

    glUniformMatrix4fv(
        glGetUniformLocation(wireProgram, "mvp"),
        1, GL_FALSE, &mvp[0][0]
    );

    glBindVertexArray(cubeVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);

    glEnable(GL_DEPTH_TEST);

    glEnable(GL_DEPTH_TEST);
}