#pragma once

#include <glm/glm.hpp>

class Renderer {
public:
    Renderer(int w, int h);

    void render();

    void onMouseDrag(float dx, float dy);
    void onZoom(float delta);

    void toggleDebug();

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
    unsigned int volumeProgram;
    unsigned int raycastProgram;

    // -----------------------------
    // VOLUME
    // -----------------------------
    unsigned int volumeTex;
    int volumeWidth = 0;
    int volumeHeight = 0;
    int volumeDepth = 0;

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

    // -----------------------------
    // METHODS
    // -----------------------------
    void initCube();
    void initShaders();
    void initVolume();

    glm::mat4 getView();
    glm::mat4 getProj();
};