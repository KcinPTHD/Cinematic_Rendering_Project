#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "renderer.h"
#include "debug_logger.h"
#include "dataset_manager.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>

int width = 800, height = 600;

Renderer* renderer;

bool isFullscreen = false;
int windowedPosX = 0, windowedPosY = 0;
int windowedWidth = 800, windowedHeight = 600;

void framebuffer_size(GLFWwindow* window, int w, int h) {
    glViewport(0, 0, w, h);
}

// ============================================================
// MAIN
// ============================================================
int main() {
    glfwInit();

    DebugLogger::init("debug.txt");

    GLFWwindow* window = glfwCreateWindow(width, height, "Volume Renderer", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    gladLoadGL();

    glEnable(GL_DEPTH_TEST);

    renderer = new Renderer(width, height);

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    // ImGui_ImplGlfw_InitForOpenGL already installs its own mouse callbacks
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Do NOT call glfwSetMouseButtonCallback / glfwSetCursorPosCallback / glfwSetScrollCallback
    // as they would replace ImGui's callbacks and break mouse input in ImGui.
    // Instead, we poll mouse state each frame (see below).

    glfwSetFramebufferSizeCallback(window, framebuffer_size);

    DatasetManager datasetManager;
    bool datasetLoaded = false;
    std::string currentDataset = "";

    std::atomic<bool> converting{false};
    std::atomic<bool> conversionSuccess{false};
    std::string convertingDatasetName;
    std::thread conversionThread;
    bool conversionFinished = false;

    // Popup states
    bool showHelp = false;
    bool showExitConfirm = false;

    while (!glfwWindowShouldClose(window)) {
        // Scale font with window width
        float baseWidth = 1200.0f;
        float scale = std::max(0.7f, std::min(2.0f, io.DisplaySize.x / baseWidth));
        io.FontGlobalScale = scale;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ============================================================
        // POLL MOUSE FOR 3D VIEW (only when ImGui doesn't capture it)
        // ============================================================
        if (!io.WantCaptureMouse) {
            // Mouse drag (left button)
            if (io.MouseDown[0]) {
                float dx = io.MouseDelta.x;
                float dy = io.MouseDelta.y;
                if (dx != 0.0f || dy != 0.0f) {
                    renderer->onMouseDrag(dx, dy);
                }
            }

            // Mouse wheel zoom
            if (io.MouseWheel != 0.0f) {
                renderer->onZoom(io.MouseWheel);
            }
        }

        // ============================================================
        // 1. GLOBAL KEYBOARD SHORTCUTS (F11, ESC, H)
        // ============================================================
        if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
            isFullscreen = !isFullscreen;
            if (isFullscreen) {
                glfwGetWindowPos(window, &windowedPosX, &windowedPosY);
                glfwGetWindowSize(window, &windowedWidth, &windowedHeight);
                GLFWmonitor* monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            } else {
                glfwSetWindowMonitor(window, nullptr, windowedPosX, windowedPosY, windowedWidth, windowedHeight, 0);
            }
        }

        // ESC: if in visualization, go back to menu; if in menu, ask to quit
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if (datasetLoaded) {
                datasetLoaded = false;
            } else {
                if (!ImGui::IsPopupOpen("Exit?")) {
                    showExitConfirm = true;
                    ImGui::OpenPopup("Exit?");
                }
            }
        }

        // H: open help popup (only if not already open)
        if (ImGui::IsKeyPressed(ImGuiKey_H)) {
            if (!ImGui::IsPopupOpen("Help") && !showHelp) {
                showHelp = true;
                ImGui::OpenPopup("Help");
            }
        }

        // ============================================================
        // 2. HELP POPUP
        // ============================================================
        if (showHelp) {
            ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(600, 500));
            if (ImGui::BeginPopupModal("Help", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("CONTROLS:");
                ImGui::BulletText("D - Toggle debug console");
                ImGui::BulletText("F - Toggle wireframe");
                ImGui::BulletText("Q/W - Decrease/Increase threshold (Shift: fine)");
                ImGui::BulletText("A/S - Decrease/Increase density (Shift: fine)");
                ImGui::BulletText("Z/X - Decrease/Increase brightness");
                ImGui::BulletText("H - Show this help");
                ImGui::BulletText("F11 - Toggle fullscreen");
                ImGui::BulletText("ESC - Return to menu (visualization) or quit (menu)");
                ImGui::BulletText("Mouse: Drag to rotate / Scroll to zoom");
                ImGui::Separator();
                ImGui::Text("Press ENTER or click Close.");
                if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                    showHelp = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            } else {
                showHelp = false;
            }
        }

        // ============================================================
        // 3. EXIT CONFIRMATION POPUP
        // ============================================================
        if (showExitConfirm) {
            ImGui::SetNextWindowSizeConstraints(ImVec2(300, 100), ImVec2(500, 200));
            if (ImGui::BeginPopupModal("Exit?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Are you sure you want to exit?");
                ImGui::Separator();
                if (ImGui::Button("Yes") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                    showExitConfirm = false;
                    ImGui::CloseCurrentPopup();
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::SameLine();
                if (ImGui::Button("No")) {
                    showExitConfirm = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            } else {
                showExitConfirm = false;
            }
        }

        // ============================================================
        // 4. MAIN MENU (if no dataset loaded)
        // ============================================================
        if (!datasetLoaded) {
            auto datasets = datasetManager.getDatasets();

            // Helper lambda to trigger load/convert
            auto activateDataset = [&](const DatasetManager::Dataset& ds) {
                if (!ds.isReady) {
                    converting = true;
                    conversionSuccess = false;
                    convertingDatasetName = ds.name;
                    conversionFinished = false;

                    conversionThread = std::thread([&datasetManager, ds, &converting, &conversionSuccess, &conversionFinished]() {
                        bool success = datasetManager.convertDataset(ds.name);
                        if (success) {
                            datasetManager.scanDatasets();
                        }
                        conversionSuccess = success;
                        converting = false;
                        conversionFinished = true;
                    });
                    conversionThread.detach();
                } else {
                    bool success = renderer->loadDataset(ds.name);
                    if (success) {
                        currentDataset = ds.name;
                        datasetLoaded = true;
                        std::cout << "[INFO] Dataset '" << ds.name << "' loaded successfully!" << std::endl;
                    } else {
                        std::cerr << "[ERROR] Failed to load dataset '" << ds.name << "'" << std::endl;
                        ImGui::OpenPopup("Error loading");
                    }
                }
            };

            // Window sizing
            float winWidth = io.DisplaySize.x * 0.4f;
            float winHeight = io.DisplaySize.y * 0.5f;
            if (winWidth < 300) winWidth = 300;
            if (winHeight < 200) winHeight = 200;
            float maxWinWidth = io.DisplaySize.x * 0.8f;
            float maxWinHeight = io.DisplaySize.y * 0.8f;
            if (winWidth > maxWinWidth) winWidth = maxWinWidth;
            if (winHeight > maxWinHeight) winHeight = maxWinHeight;

            ImGui::SetNextWindowSize(ImVec2(winWidth, winHeight), ImGuiCond_Always);
            ImGui::SetNextWindowSizeConstraints(ImVec2(300, 100), ImVec2(maxWinWidth, maxWinHeight));
            ImGui::Begin("Select Dataset", nullptr,
                         ImGuiWindowFlags_NoCollapse);

            ImGui::TextWrapped("Press H for help.");

            if (converting) {
                static float timer = 0.0f;
                timer += ImGui::GetIO().DeltaTime;
                int dots = (int)(timer * 2.0f) % 4;
                std::string msg = "Converting" + std::string(dots, '.') + std::string(3 - dots, ' ');
                ImGui::TextWrapped("%s", msg.c_str());
                ImGui::TextWrapped("Please wait...");
                ImGui::BeginDisabled();
                ImGui::Button("Convert");
                ImGui::EndDisabled();
            } else {
                if (conversionFinished) {
                    conversionFinished = false;
                    if (conversionSuccess) {
                        auto updatedDatasets = datasetManager.getDatasets();
                        for (const auto& ds : updatedDatasets) {
                            if (ds.name == convertingDatasetName && ds.isReady) {
                                bool success = renderer->loadDataset(ds.name);
                                if (success) {
                                    currentDataset = ds.name;
                                    datasetLoaded = true;
                                    std::cout << "[INFO] Dataset '" << ds.name << "' loaded successfully!" << std::endl;
                                } else {
                                    std::cerr << "[ERROR] Failed to load dataset '" << ds.name << "'" << std::endl;
                                    ImGui::OpenPopup("Error loading");
                                }
                                break;
                            }
                        }
                    } else {
                        std::cerr << "[ERROR] Conversion failed for dataset '" << convertingDatasetName << "'" << std::endl;
                        ImGui::OpenPopup("Error loading");
                    }
                    convertingDatasetName = "";
                }

                if (datasets.empty()) {
                    ImGui::TextWrapped("No datasets found in data/");
                    ImGui::TextWrapped("Place folders with .dcm files in data/");
                } else {
                    ImGui::TextWrapped("Click a dataset below to load it (or convert it first).");
                    ImGui::Spacing();
                    for (size_t i = 0; i < datasets.size(); ++i) {
                        const auto& ds = datasets[i];
                        std::string label = ds.name + (ds.isReady ? " (ready)" : " (needs conversion)");

                        ImGui::PushID((int)i);
                        if (!ds.isReady) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
                        }
                        if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_None)) {
                            activateDataset(ds);
                        }
                        if (!ds.isReady) {
                            ImGui::PopStyleColor();
                        }
                        ImGui::PopID();
                    }
                }
            }

            // Error popup (Enter closes it)
            if (ImGui::BeginPopupModal("Error loading", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Failed to load the dataset.");
                ImGui::Text("Check if the .raw and .txt files exist.");
                if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::End();
        }

        // ============================================================
        // 5. RENDER 3D VIEW (if dataset loaded)
        // ============================================================
        if (datasetLoaded) {
            if (ImGui::IsKeyPressed(ImGuiKey_D)) renderer->toggleDebug();
            if (ImGui::IsKeyPressed(ImGuiKey_F)) renderer->toggleWireframe();

            if (ImGui::IsKeyPressed(ImGuiKey_Q)) {
                float step = (io.KeyShift) ? -0.001f : -0.01f;
                renderer->adjustThreshold(step);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_W)) {
                float step = (io.KeyShift) ? 0.001f : 0.01f;
                renderer->adjustThreshold(step);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_A)) {
                float step = (io.KeyShift) ? -0.001f : -0.01f;
                renderer->adjustDensity(step);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_S)) {
                float step = (io.KeyShift) ? 0.001f : 0.01f;
                renderer->adjustDensity(step);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Z)) renderer->adjustBrightness(-0.1f);
            if (ImGui::IsKeyPressed(ImGuiKey_X)) renderer->adjustBrightness(+0.1f);

            renderer->render();
        } else {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    DebugLogger::close();
    return 0;
}