#include "renderer.h"
#include "volume_loader.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <ctime>

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
    std::ofstream log("debug_volume.txt");

    std::string rawPath = "data/ct.raw";
    std::string metaPath = "data/ct.txt";

    log << "=== INIT VOLUME START ===\n";

    // -----------------------------
    // CHECK FILES
    // -----------------------------
    std::ifstream meta(metaPath);
    if (!meta)
    {
        log << "[ERROR] Missing meta file: " << metaPath << "\n";
        throw std::runtime_error("Missing ct.txt");
    }

    std::ifstream raw(rawPath, std::ios::binary);
    if (!raw)
    {
        log << "[ERROR] Missing RAW file: " << rawPath << "\n";
        throw std::runtime_error("Failed to open RAW file");
    }

    log << "[OK] Files found\n";

    // -----------------------------
    // READ DIMS
    // -----------------------------
    int W, H, D;
    meta >> W >> H >> D;

    log << "Dims read: " << W << " x " << H << " x " << D << "\n";

    if (W <= 0 || H <= 0 || D <= 0)
    {
        log << "[ERROR] Invalid dimensions\n";
        throw std::runtime_error("Invalid volume dimensions");
    }

    // -----------------------------
    // LOAD RAW
    // -----------------------------
    Volume vol;
    vol.width = W;
    vol.height = H;
    vol.depth = D;
    vol.data.resize(W * H * D);

    raw.read(reinterpret_cast<char*>(vol.data.data()),
             vol.data.size() * sizeof(float));

    log << "Expected bytes: " << vol.data.size() * sizeof(float) << "\n";
    log << "Bytes actually read: " << raw.gcount() << "\n";

    if (!raw)
    {
        log << "[WARNING] RAW read may be incomplete\n";
    }

    // -----------------------------
    // DATA VALIDATION
    // -----------------------------
    float minV = 1e9f;
    float maxV = -1e9f;
    float avg = 0.0f;

    for (size_t i = 0; i < vol.data.size(); i++)
    {
        float v = vol.data[i];

        if (std::isnan(v) || std::isinf(v))
        {
            log << "[ERROR] NaN/INF detected at index " << i << "\n";
            throw std::runtime_error("Invalid voxel data");
        }

        minV = std::min(minV, v);
        maxV = std::max(maxV, v);
        avg += v;
    }

    avg /= vol.data.size();

    log << "Data stats:\n";
    log << "  Min: " << minV << "\n";
    log << "  Max: " << maxV << "\n";
    log << "  Avg: " << avg << "\n";

    // -----------------------------
    // DEBUG MELHORADO (substitui os Sample voxels aleatórios)
    // -----------------------------
    log << "Non-zero voxel analysis:\n";

    size_t zeroCount = 0;
    size_t nonZeroCount = 0;
    float maxValue = -1e9f;
    size_t maxIndex = 0;

    // 1ª passagem: contagem de zeros e localização do máximo
    for (size_t i = 0; i < vol.data.size(); ++i)
    {
        float v = vol.data[i];
        if (v == 0.0f)
            zeroCount++;
        else
        {
            nonZeroCount++;
            if (v > maxValue)
            {
                maxValue = v;
                maxIndex = i;
            }
        }
    }

    log << "  Zero voxels: " << zeroCount << "\n";
    log << "  Non-zero voxels: " << nonZeroCount << "\n";

    if (nonZeroCount > 0)
    {
        unsigned int z = maxIndex / (vol.width * vol.height);
        unsigned int y = (maxIndex % (vol.width * vol.height)) / vol.width;
        unsigned int x = (maxIndex % (vol.width * vol.height)) % vol.width;
        log << "  Max value: " << maxValue << " at index " << maxIndex
            << " (" << x << "," << y << "," << z << ")\n";
    }
    else
    {
        log << "  Max value: N/A (volume all zeros?)\n";
    }

    // Mostrar os primeiros 10 voxels não-nulos com as suas coordenadas
    log << "First 10 non-zero voxels:\n";
    int printed = 0;
    for (size_t i = 0; i < vol.data.size() && printed < 10; ++i)
    {
        float v = vol.data[i];
        if (v > 0.0f)
        {
            unsigned int z = i / (vol.width * vol.height);
            unsigned int y = (i % (vol.width * vol.height)) / vol.width;
            unsigned int x = (i % (vol.width * vol.height)) % vol.width;
            log << "  [" << i << "] = " << v
                << "  (" << x << "," << y << "," << z << ")\n";
            printed++;
        }
    }
    if (printed == 0)
        log << "  (no non-zero voxels found)\n";

    // -----------------------------
    // UPLOAD GPU
    // -----------------------------
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
        vol.width,
        vol.height,
        vol.depth,
        0,
        GL_RED,
        GL_FLOAT,
        vol.data.data()
    );

    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        log << "[OPENGL ERROR] glTexImage3D failed: " << err << "\n";
    else
        log << "[OK] Texture uploaded to GPU\n";

    volumeWidth = vol.width;
    volumeHeight = vol.height;
    volumeDepth = vol.depth;

    log << "Final volume dims set in renderer: "
        << volumeWidth << " "
        << volumeHeight << " "
        << volumeDepth << "\n";

    // DIAGNOSTIC: Sample some voxels to verify data is in texture
    log << "\n=== TEXTURE DIAGNOSTIC ===\n";
    log << "Data distribution analysis:\n";
    
    // Count voxels per z-slice
    std::vector<int> sliceNonZero(vol.depth, 0);
    for (size_t i = 0; i < vol.data.size(); i++)
    {
        if (vol.data[i] > 0.0f)
        {
            unsigned int z = i / (vol.width * vol.height);
            if (z < vol.depth)
                sliceNonZero[z]++;
        }
    }
    
    log << "Non-zero voxel counts per slice (first 10 slices):\n";
    for (int z = 0; z < 10 && z < vol.depth; z++)
    {
        log << "  Slice " << z << ": " << sliceNonZero[z] << " non-zero voxels\n";
    }
    
    // Sample from known non-zero region (around slice 73, x~191, y~273)
    log << "\nSampling from known data region (around index 19276479):\n";
    for (int dz = -2; dz <= 2; dz++)
    {
        int z = 73 + dz;
        if (z >= 0 && z < vol.depth)
        {
            for (int dy = -2; dy <= 2; dy++)
            {
                int y = 273 + dy;
                if (y >= 0 && y < vol.height)
                {
                    int x = 191;
                    size_t idx = z * vol.width * vol.height + y * vol.width + x;
                    if (idx < vol.data.size())
                        log << "  (" << x << "," << y << "," << z << ") = " << vol.data[idx] << "\n";
                }
            }
        }
    }
    
    log << "=== INIT VOLUME END ===\n";
    log.close();
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

    // Cube centered at origin: from -scale/2 to +scale/2
    // This ensures proper ray-casting alignment
    float hx = scaleX / 2.0f;
    float hy = scaleY / 2.0f;
    float hz = scaleZ / 2.0f;

    float vertices[] = {
        -hx,-hy,-hz,  hx,-hy,-hz,  hx,hy,-hz,  -hx,hy,-hz,
        -hx,-hy,hz,   hx,-hy,hz,   hx,hy,hz,   -hx,hy,hz
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

void Renderer::toggleWireframe() {
    wireframeEnabled = !wireframeEnabled;
    std::cout << "[WIREFRAME] " << (wireframeEnabled ? "ON" : "OFF") << std::endl;
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

    // Cube is now centered at origin
    glm::vec3 c(0.0f, 0.0f, 0.0f);

    glm::vec3 p;
    p.x = c.x + distance * cos(pitch) * sin(yaw);
    p.y = c.y + distance * sin(pitch);
    p.z = c.z + distance * cos(pitch) * cos(yaw);

    return glm::lookAt(p, c, glm::vec3(0,1,0));
}

glm::mat4 Renderer::getProj() {
    int w, h;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &w, &h);

    // Guard against invalid window size
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    float aspect = static_cast<float>(w) / static_cast<float>(h);
    
    return glm::perspective(glm::radians(45.0f),
                            aspect,
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
        std::cout << "Cube range: [-" << scaleX/2.0f << "," << scaleX/2.0f << "] x "
                  << "[-" << scaleY/2.0f << "," << scaleY/2.0f << "] x "
                  << "[-" << scaleZ/2.0f << "," << scaleZ/2.0f << "]" << std::endl;
        std::cout << "Camera: dist=" << distance << ", yaw=" << yaw << ", pitch=" << pitch << std::endl;
        std::cout << "Center: (0, 0, 0)" << std::endl;
        std::cout << "Threshold: " << threshold << ", Density: " << density << ", Brightness: " << brightness << std::endl;
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
    // 2. DRAW WIREFRAME (no z-fighting)
    // -----------------------------
    if (wireframeEnabled)
    {
        glDisable(GL_DEPTH_TEST);

        glUseProgram(wireProgram);

        glUniformMatrix4fv(
            glGetUniformLocation(wireProgram, "mvp"),
            1, GL_FALSE, &mvp[0][0]
        );

        glBindVertexArray(cubeVAO);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);

        glEnable(GL_DEPTH_TEST);
    }
}
