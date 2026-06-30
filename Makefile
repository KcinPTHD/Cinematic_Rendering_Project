# ============================================================
# Project settings
# ============================================================
TARGET      := app
BIN_DIR     := bin
SRC_DIR     := src

# ============================================================
# OpenGL / GLFW / GLM / GLAD paths (assumindo instalados no sistema)
# GLAD está em /mnt/c/OpenGL/glad (ou podes copiá-lo para o projeto)
# ============================================================
GLAD_DIR    := /mnt/c/OpenGL/glad

# ============================================================
# Dear ImGui
# ============================================================
IMGUI_DIR   := external/imgui-1.92.8
IMGUI_SRC   := $(wildcard $(IMGUI_DIR)/*.cpp) \
               $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp \
               $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

# ============================================================
# Compiler (nativo Linux)
# ============================================================
CXX         := g++
CXXFLAGS    := -g -Wall -std=c++17 \
               -I$(GLAD_DIR)/include \
               -I/usr/include/glm \
               -I$(IMGUI_DIR) \
               -I$(IMGUI_DIR)/backends

LDFLAGS     := -lglfw -lGL -ldl -lm

# ============================================================
# Sources
# ============================================================
SRC := $(wildcard $(SRC_DIR)/*.cpp) $(GLAD_DIR)/src/glad.c $(IMGUI_SRC)
OUT := $(BIN_DIR)/$(TARGET)

# ============================================================
# Python venv (apenas para dependências, não para conversão automática)
# ============================================================
VENV_DIR    := .venv
PYTHON      := $(VENV_DIR)/bin/python
PIP         := $(VENV_DIR)/bin/pip

# ============================================================
# Rules
# ============================================================
all: venv $(OUT)

venv:
	@if [ ! -d $(VENV_DIR) ]; then \
		echo "Creating virtual environment..."; \
		python3 -m venv $(VENV_DIR); \
		echo "Installing dependencies..."; \
		$(PIP) install pydicom numpy; \
	else \
		echo "Virtual environment already exists."; \
	fi

$(OUT): $(SRC) | $(BIN_DIR)
	@echo "Compiling for Linux (WSL) with $(CXX)"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Build successful -> $(OUT)"

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# ============================================================
# Utility targets
# ============================================================
# Converte manualmente (se quiseres forçar a conversão do dataset 'ct')
convert: venv
	$(PYTHON) utils/convert_to_raw.py --input-dir data/ct --output-prefix data/ct --auto-detect

app: $(OUT)

clean:
	rm -rf $(BIN_DIR)
	rm -f data/*.raw data/*.txt

clean-venv:
	rm -rf $(VENV_DIR)

run: all
	./$(OUT)

.PHONY: all venv convert app clean clean-venv run