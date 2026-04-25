#pragma once

#include <glm/glm.hpp>

class Renderer {
public:
    Renderer(int w, int h);

    void render();

    void onMouseDrag(float dx, float dy);
    void onZoom(float delta);

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

    // -----------------------------
    // VOLUME
    // -----------------------------
    unsigned int volumeTex;

    // -----------------------------
    // CAMERA
    // -----------------------------
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 2.5f;

    // -----------------------------
    // METHODS
    // -----------------------------
    void initCube();
    void initShaders();
    void initVolume();

    glm::mat4 getView();
    glm::mat4 getProj();
};