#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "renderer.h"

#include <iostream>

int width = 800, height = 600;

Renderer* renderer;

bool mousePressed = false;
double lastX = 0.0, lastY = 0.0;

void mouse_button(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        mousePressed = (action == GLFW_PRESS);

        glfwGetCursorPos(window, &lastX, &lastY);
    }
}

void cursor_pos(GLFWwindow* window, double xpos, double ypos) {
    if (!mousePressed) return;

    float dx = static_cast<float>(xpos - lastX);
    float dy = static_cast<float>(ypos - lastY);

    lastX = xpos;
    lastY = ypos;

    renderer->onMouseDrag(dx, dy);
}

void scroll(GLFWwindow* window, double xoffset, double yoffset) {
    renderer->onZoom(static_cast<float>(yoffset));
}

void framebuffer_size(GLFWwindow* window, int w, int h) {
    glViewport(0, 0, w, h);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
{

    if (key == GLFW_KEY_D && action == GLFW_PRESS) {renderer->toggleDebug();}
    if (key == GLFW_KEY_Q) renderer->adjustThreshold(-0.01f);
    if (key == GLFW_KEY_W) renderer->adjustThreshold(+0.01f);

    if (key == GLFW_KEY_A) renderer->adjustDensity(-0.01f);
    if (key == GLFW_KEY_S) renderer->adjustDensity(+0.01f);

    if (key == GLFW_KEY_Z) renderer->adjustBrightness(-0.1f);
    if (key == GLFW_KEY_X) renderer->adjustBrightness(+0.1f);
}
}

int main() {
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(width, height, "Volume Renderer", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    gladLoadGL();

    glEnable(GL_DEPTH_TEST);

    renderer = new Renderer(width, height);

    glfwSetMouseButtonCallback(window, mouse_button);
    glfwSetCursorPosCallback(window, cursor_pos);
    glfwSetScrollCallback(window, scroll);
    glfwSetFramebufferSizeCallback(window, framebuffer_size);
    glfwSetKeyCallback(window, key_callback);

    while (!glfwWindowShouldClose(window)) {
        renderer->render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}