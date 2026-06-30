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

int width = 800, height = 600;

Renderer* renderer;

bool mousePressed = false;
double lastX = 0.0, lastY = 0.0;

// ============================================================
// CALLBACKS DO MOUSE (drag) – com verificação do ImGui
// ============================================================
void mouse_button(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (!ImGui::GetIO().WantCaptureMouse) {
            mousePressed = (action == GLFW_PRESS);
            if (mousePressed) {
                glfwGetCursorPos(window, &lastX, &lastY);
            }
        } else {
            mousePressed = false;
        }
    }
}

void cursor_pos(GLFWwindow* window, double xpos, double ypos) {
    if (!mousePressed) return;
    if (ImGui::GetIO().WantCaptureMouse) return;

    float dx = static_cast<float>(xpos - lastX);
    float dy = static_cast<float>(ypos - lastY);

    lastX = xpos;
    lastY = ypos;

    renderer->onMouseDrag(dx, dy);
}

void scroll(GLFWwindow* window, double xoffset, double yoffset) {
    if (!ImGui::GetIO().WantCaptureMouse) {
        renderer->onZoom(static_cast<float>(yoffset));
    }
}

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

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    glfwSetMouseButtonCallback(window, mouse_button);
    glfwSetCursorPosCallback(window, cursor_pos);
    glfwSetScrollCallback(window, scroll);
    glfwSetFramebufferSizeCallback(window, framebuffer_size);

    DatasetManager datasetManager;
    bool datasetLoaded = false;
    std::string currentDataset = "";
    int selectedIndex = 0;

    // Estado da conversão assíncrona
    std::atomic<bool> converting{false};
    std::atomic<bool> conversionSuccess{false};
    std::string convertingDatasetName;
    std::thread conversionThread;
    bool conversionFinished = false;

    while (!glfwWindowShouldClose(window)) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Verificar se a thread de conversão terminou
        if (converting && conversionThread.joinable()) {
            // Verificar se a thread já terminou (joinable é false após join)
            // Como usamos detach, não podemos dar join. Melhor usar uma flag.
            // Vamos usar uma flag definida pela thread.
        }

        if (!datasetLoaded) {
            auto datasets = datasetManager.getDatasets();

            // Navegação por teclado (apenas se não estiver a converter)
            if (!converting) {
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && !datasets.empty()) {
                    selectedIndex = (selectedIndex + 1) % datasets.size();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && !datasets.empty()) {
                    selectedIndex = (selectedIndex - 1 + datasets.size()) % datasets.size();
                }

                if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !datasets.empty()) {
                    const auto& ds = datasets[selectedIndex];
                    if (!ds.isReady) {
                        // Iniciar conversão em thread separada
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
                        // Dataset já pronto, carregar diretamente
                        bool success = renderer->loadDataset(ds.name);
                        if (success) {
                            currentDataset = ds.name;
                            datasetLoaded = true;
                            std::cout << "[INFO] Dataset '" << ds.name << "' carregado com sucesso!" << std::endl;
                        } else {
                            std::cerr << "[ERROR] Falha ao carregar dataset '" << ds.name << "'" << std::endl;
                            ImGui::OpenPopup("Erro ao carregar");
                        }
                    }
                }
            }

            // Calcular tamanho da janela com base no tamanho da tela
            float winWidth = io.DisplaySize.x * 0.4f;
            float winHeight = io.DisplaySize.y * 0.5f;
            if (winWidth < 300) winWidth = 300;
            if (winHeight < 200) winHeight = 200;
            // Limitar máximo a 80% da janela para não ocupar tudo
            float maxWinWidth = io.DisplaySize.x * 0.8f;
            float maxWinHeight = io.DisplaySize.y * 0.8f;
            if (winWidth > maxWinWidth) winWidth = maxWinWidth;
            if (winHeight > maxWinHeight) winHeight = maxWinHeight;

            ImGui::SetNextWindowSize(ImVec2(winWidth, winHeight), ImGuiCond_Always);
            ImGui::SetNextWindowSizeConstraints(ImVec2(300, 100), ImVec2(maxWinWidth, maxWinHeight));
            ImGui::Begin("Selecionar Dataset", nullptr,
                         ImGuiWindowFlags_NoCollapse);

            if (converting) {
                // Mostrar animação de carregamento
                static float timer = 0.0f;
                timer += ImGui::GetIO().DeltaTime;
                int dots = (int)(timer * 2.0f) % 4;
                std::string msg = "A converter" + std::string(dots, '.') + std::string(3 - dots, ' ');
                ImGui::TextWrapped("%s", msg.c_str());
                ImGui::TextWrapped("Aguarde...");
                // Impedir interação
                ImGui::BeginDisabled();
                ImGui::Button("Converter");
                ImGui::EndDisabled();
            } else {
                // Verificar se a conversão terminou e processar resultado
                if (conversionFinished) {
                    conversionFinished = false;
                    if (conversionSuccess) {
                        // Recarregar datasets para atualizar estado
                        auto updatedDatasets = datasetManager.getDatasets();
                        // Procurar o dataset convertido
                        for (const auto& ds : updatedDatasets) {
                            if (ds.name == convertingDatasetName && ds.isReady) {
                                bool success = renderer->loadDataset(ds.name);
                                if (success) {
                                    currentDataset = ds.name;
                                    datasetLoaded = true;
                                    std::cout << "[INFO] Dataset '" << ds.name << "' carregado com sucesso!" << std::endl;
                                } else {
                                    std::cerr << "[ERROR] Falha ao carregar dataset '" << ds.name << "'" << std::endl;
                                    ImGui::OpenPopup("Erro ao carregar");
                                }
                                break;
                            }
                        }
                    } else {
                        std::cerr << "[ERROR] Falha na conversão do dataset '" << convertingDatasetName << "'" << std::endl;
                        ImGui::OpenPopup("Erro ao carregar");
                    }
                    convertingDatasetName = "";
                }

                if (datasets.empty()) {
                    ImGui::TextWrapped("Nenhum dataset encontrado em data/");
                    ImGui::TextWrapped("Coloque pastas com ficheiros .dcm em data/");
                } else {
                    for (size_t i = 0; i < datasets.size(); ++i) {
                        const auto& ds = datasets[i];
                        std::string label = ds.name + (ds.isReady ? " (pronto)" : " (converter)");
                        if ((int)i == selectedIndex) {
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "> %s", label.c_str());
                        } else {
                            ImGui::Text("  %s", label.c_str());
                        }
                    }
                    ImGui::Text("\nUse setas UP DOWN para selecionar e ENTER para visualizar.");
                }
            }

            if (ImGui::BeginPopupModal("Erro ao carregar", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Não foi possível carregar o dataset.");
                ImGui::Text("Verifique se os ficheiros .raw e .txt existem.");
                if (ImGui::Button("OK")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::End();
        }

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

    // Se a thread de conversão ainda estiver a correr, esperar (mas detach já libertou)
    // Não precisamos de fazer join porque demos detach.

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    DebugLogger::close();
    return 0;
}