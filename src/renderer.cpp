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
    // Inicializar a textura a 0 (ainda sem volume carregado)
    volumeTex = 0;
    volumeWidth = 0;
    volumeHeight = 0;
    volumeDepth = 0;
    
    // Inicializar VAO/VBO/EBO a 0 para evitar deleções inválidas
    cubeVAO = 0;
    cubeVBO = 0;
    cubeEBO = 0;
    
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
    // META FORMAT: "width height depth spacingW spacingH spacingD" (mm).
    // As contagens já vêm na ordem exata que glTexImage3D(width,height,depth,...)
    // espera (ver convert_to_raw.py para a explicação completa do porquê do
    // transpose (h,d,w)). O espaçamento físico segue a MESMA ordem de eixos.
    int W, H, D;
    float spacingW = 1.0f, spacingH = 1.0f, spacingD = 1.0f;
    meta >> W >> H >> D;
    if (!(meta >> spacingW >> spacingH >> spacingD)) {
        // Compatibilidade com metas antigos sem espaçamento: assume isotrópico
        spacingW = spacingH = spacingD = 1.0f;
        log << "[WARNING] Meta sem espaçamento físico; a assumir voxels cúbicos\n";
    }
    voxelSpacingX = spacingW;
    voxelSpacingY = spacingH;
    voxelSpacingZ = spacingD;

    log << "Dims read: " << W << " x " << H << " x " << D << "\n";
    log << "Spacing read (mm): " << spacingW << " x " << spacingH << " x " << spacingD << "\n";

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
        int z = maxIndex / (vol.width * vol.height);
        int y = (maxIndex % (vol.width * vol.height)) / vol.width;
        int x = (maxIndex % (vol.width * vol.height)) % vol.width;
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
            int z = i / (vol.width * vol.height);
            int y = (i % (vol.width * vol.height)) / vol.width;
            int x = (i % (vol.width * vol.height)) % vol.width;
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
    voxelSpacingX = spacingW;
    voxelSpacingY = spacingH;
    voxelSpacingZ = spacingD;

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
            int z = i / (vol.width * vol.height);
            if (z < (int)vol.depth)
                sliceNonZero[z]++;
        }
    }
    
    log << "Non-zero voxel counts per slice (first 10 slices):\n";
    for (int z = 0; z < 10 && z < (int)vol.depth; z++)
    {
        log << "  Slice " << z << ": " << sliceNonZero[z] << " non-zero voxels\n";
    }
    
    // Sample from known non-zero region (around slice 73, x~191, y~273)
    log << "\nSampling from known data region (around index 19276479):\n";
    for (int dz = -2; dz <= 2; dz++)
    {
        int z = 73 + dz;
        if (z >= 0 && z < (int)vol.depth)
        {
            for (int dy = -2; dy <= 2; dy++)
            {
                int y = 273 + dy;
                if (y >= 0 && y < (int)vol.height)
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
    
    // Reconstruir o cubo com as novas dimensões
    initCube();
}

// =====================================
// CUBE (BOUNDING BOX) - adapta-se às dimensões do volume
// =====================================
// =====================================
// PHYSICAL SCALE (conta voxels * espaçamento real em mm)
// =====================================
glm::vec3 Renderer::computeCubeScale() {
    float physW = static_cast<float>(volumeWidth)  * voxelSpacingX;
    float physH = static_cast<float>(volumeHeight) * voxelSpacingY;
    float physD = static_cast<float>(volumeDepth)  * voxelSpacingZ;

    float maxDim = physW;
    if (physH > maxDim) maxDim = physH;
    if (physD > maxDim) maxDim = physD;
    if (maxDim <= 0.0f) maxDim = 1.0f;

    return glm::vec3(physW / maxDim, physH / maxDim, physD / maxDim);
}

void Renderer::initCube() {
    // Delete old buffers if they exist
    if (cubeVAO) glDeleteVertexArrays(1, &cubeVAO);
    if (cubeVBO) glDeleteBuffers(1, &cubeVBO);
    if (cubeEBO) glDeleteBuffers(1, &cubeEBO);
    cubeVAO = cubeVBO = cubeEBO = 0;

    // Usar o tamanho FÍSICO real do volume (contagem*espaçamento em mm,
    // normalizado para que o maior eixo = 1) em vez da contagem de
    // voxels em bruto. Isto evita "esmagar" eixos quando o espaçamento
    // entre cortes é diferente do espaçamento in-plane.
    glm::vec3 scale = computeCubeScale();
    float scaleX = scale.x;
    float scaleY = scale.y;
    float scaleZ = scale.z;

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
// LOAD DATASET
// =====================================
bool Renderer::loadDataset(const std::string& name) {
    std::string rawPath = "data/" + name + ".raw";
    std::string metaPath = "data/" + name + ".txt";

    std::cout << "[Renderer] loadDataset: " << name << std::endl;
    std::cout << "  rawPath: " << rawPath << std::endl;
    std::cout << "  metaPath: " << metaPath << std::endl;

    // Verificar existência
    std::ifstream meta(metaPath);
    if (!meta) {
        std::cerr << "[Renderer] Meta file not found: " << metaPath << std::endl;
        return false;
    }
    std::ifstream raw(rawPath, std::ios::binary);
    if (!raw) {
        std::cerr << "[Renderer] RAW file not found: " << rawPath << std::endl;
        return false;
    }

    // Ler dimensões
    // META FORMAT: "width height depth spacingW spacingH spacingD" (mm)
    int W, H, D;
    float spacingW = 1.0f, spacingH = 1.0f, spacingD = 1.0f;
    meta >> W >> H >> D;
    if (!(meta >> spacingW >> spacingH >> spacingD)) {
        spacingW = spacingH = spacingD = 1.0f;
        std::cout << "[Renderer] Meta sem espaçamento físico; a assumir voxels cúbicos" << std::endl;
    }
    if (W <= 0 || H <= 0 || D <= 0) {
        std::cerr << "[Renderer] Invalid dimensions in " << metaPath << std::endl;
        return false;
    }

    // Carregar dados
    Volume vol;
    vol.width = W;
    vol.height = H;
    vol.depth = D;
    vol.data.resize(W * H * D);
    raw.read(reinterpret_cast<char*>(vol.data.data()), vol.data.size() * sizeof(float));
    if (!raw) {
        std::cerr << "[Renderer] Failed to read RAW data from " << rawPath << std::endl;
        return false;
    }

    // Validar dados (básico)
    float minV = 1e9f, maxV = -1e9f;
    for (size_t i = 0; i < vol.data.size(); i++) {
        float v = vol.data[i];
        if (std::isnan(v) || std::isinf(v)) {
            std::cerr << "[Renderer] NaN/INF detected in volume data" << std::endl;
            return false;
        }
        if (v < minV) minV = v;
        if (v > maxV) maxV = v;
    }
    std::cout << "[Renderer] Volume stats - Min: " << minV << ", Max: " << maxV << std::endl;

    // Substituir textura existente (se houver)
    if (volumeTex != 0) {
        glDeleteTextures(1, &volumeTex);
        volumeTex = 0;
    }

    // Criar nova textura
    glGenTextures(1, &volumeTex);
    glBindTexture(GL_TEXTURE_3D, volumeTex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, vol.width, vol.height, vol.depth, 0, GL_RED, GL_FLOAT, vol.data.data());

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[Renderer] OpenGL error uploading texture: " << err << std::endl;
        return false;
    }

    volumeWidth = vol.width;
    volumeHeight = vol.height;
    volumeDepth = vol.depth;
    voxelSpacingX = spacingW;
    voxelSpacingY = spacingH;
    voxelSpacingZ = spacingD;

    // Reconstruir o cubo com as novas dimensões
    initCube();

    std::cout << "[Renderer] Loaded dataset: " << name << " (" << W << "x" << H << "x" << D << ")" << std::endl;
    return true;
}

// =====================================
// RENDER
// =====================================
void Renderer::render() {
    // Se não houver volume carregado, não renderizar
    if (volumeTex == 0 || volumeWidth == 0) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }

    // Debug output (toggle com 'd')
    if (debugEnabled) {
        std::cout << "=== DEBUG ===" << std::endl;
        std::cout << "Volume: " << volumeWidth << "x" << volumeHeight << "x" << volumeDepth << std::endl;
        std::cout << "Spacing (mm): " << voxelSpacingX << "x" << voxelSpacingY << "x" << voxelSpacingZ << std::endl;

        glm::vec3 scale = computeCubeScale();
        float scaleX = scale.x;
        float scaleY = scale.y;
        float scaleZ = scale.z;

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

    // Calcular escala física (real, mm) para centered do cubo
    glm::vec3 cubeScale = computeCubeScale();
    float scaleX = cubeScale.x;
    float scaleY = cubeScale.y;
    float scaleZ = cubeScale.z;

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