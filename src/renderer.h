#pragma once

#include <glm/glm.hpp>
#include <string>

class Renderer {
public:
    Renderer(int w, int h);

    void render();

    void onMouseDrag(float dx, float dy);
    void onZoom(float delta);

    void toggleDebug();
    void toggleWireframe();

    void adjustThreshold(float v);
    void adjustDensity(float v);
    void adjustBrightness(float v);

    // Load a specific dataset (name = subdirectory in data/)
    bool loadDataset(const std::string& name);

private:
    int width, height;

    // -----------------------------
    // CUBE
    // -----------------------------
    unsigned int cubeVAO;
    unsigned int cubeVBO;
    unsigned int cubeEBO;

    // -----------------------------
    // SHADERS
    // -----------------------------
    unsigned int wireProgram;
    unsigned int raycastProgram;

    // -----------------------------
    // VOLUME
    // -----------------------------
    unsigned int volumeTex;
    unsigned int volumeWidth = 0;
    unsigned int volumeHeight = 0;
    unsigned int volumeDepth = 0;

    // Espaçamento físico real (mm) por eixo, lido do DICOM
    // (PixelSpacing / SliceThickness). Usado para que datasets com
    // voxels não-cúbicos (ex. cortes de tórax mais espessos que a
    // resolução in-plane) não fiquem "esmagados" num eixo.
    float voxelSpacingX = 1.0f;
    float voxelSpacingY = 1.0f;
    float voxelSpacingZ = 1.0f;

    // -----------------------------
    // SLIDERS
    // -----------------------------
    float threshold = 0.15f;
    float density = 0.05f;
    float brightness = 1.5f;

    // -----------------------------
    // CAMERA
    // -----------------------------
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 2.5f;

    // -----------------------------
    // DEBUG
    // -----------------------------
    bool debugEnabled = false;
    bool wireframeEnabled = true;

    // -----------------------------
    // METHODS
    // -----------------------------
    void initCube();
    void initShaders();
    void initVolume();

    // Calcula a escala normalizada do cubo (maior eixo = 1) usando o
    // tamanho FÍSICO real (contagem*espaçamento), não a contagem de
    // voxels em bruto.
    glm::vec3 computeCubeScale();

    glm::mat4 getView();
    glm::mat4 getProj();
};