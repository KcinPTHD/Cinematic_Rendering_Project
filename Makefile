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
# Usamos GLAD da instalação Windows; mas podemos também usar o GLAD gerado localmente
GLAD_DIR    := /mnt/c/OpenGL/glad

# ============================================================
# Compiler (nativo Linux)
# ============================================================
CXX         := g++
CXXFLAGS    := -g -Wall -std=c++17 \
               -I$(GLAD_DIR)/include \
               -I/usr/include/glm

LDFLAGS     := -lglfw -lGL -ldl -lm

# ============================================================
# Sources
# ============================================================
SRC := $(wildcard $(SRC_DIR)/*.cpp) $(GLAD_DIR)/src/glad.c
OUT := $(BIN_DIR)/$(TARGET)

# ============================================================
# Python venv
# ============================================================
VENV_DIR    := .venv
PYTHON      := $(VENV_DIR)/bin/python
PIP         := $(VENV_DIR)/bin/pip

CONVERT_SCRIPT := utils/convert_to_raw.py
CONVERT_FLAGS  := --auto-detect
DATA_FILES     := data/ct.raw data/ct.txt

# ============================================================
# Rules
# ============================================================
all: venv $(DATA_FILES) $(OUT)

venv:
	@if [ ! -d $(VENV_DIR) ]; then \
		echo "Creating virtual environment..."; \
		python3 -m venv $(VENV_DIR); \
		echo "Installing dependencies..."; \
		$(PIP) install pydicom numpy; \
	else \
		echo "Virtual environment already exists."; \
	fi

$(DATA_FILES): $(CONVERT_SCRIPT) | venv
	$(PYTHON) $(CONVERT_SCRIPT) $(CONVERT_FLAGS)

$(OUT): $(SRC) | $(BIN_DIR)
	@echo "Compiling for Linux (WSL) with $(CXX)"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Build successful -> $(OUT)"

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# ============================================================
# Utility targets
# ============================================================
convert: venv
	$(PYTHON) $(CONVERT_SCRIPT) $(CONVERT_FLAGS)

app: $(OUT)

clean:
	rm -rf $(BIN_DIR)
	rm -f $(DATA_FILES)

clean-venv:
	rm -rf $(VENV_DIR)

run: all
	./$(OUT)

.PHONY: all venv convert app clean clean-venv run