# Volume Renderer - Build & Run Guide

[![Demonstration Video](https://github.com/user-attachments/assets/39b0bec6-3781-4d6a-ac14-9c6496b55498)](https://github.com/user-attachments/assets/39b0bec6-3781-4d6a-ac14-9c6496b55498)

## Overview

This project implements an interactive volumetric renderer for medical datasets (CT/MRI), using ray marching with exact AABB intersection, gradient-based normal estimation, diffuse Lambertian illumination, and front-to-back compositing. It supports multiple DICOM datasets with a graphical selection interface and on-demand conversion to a raw format optimized for OpenGL 3D textures.

## Prerequisites

### Common (both platforms)
- **Git** (optional, for cloning)
- **Python 3** with `pydicom` and `numpy` installed (for DICOM conversion)
- **GLM** math library (for C++)
- **Dear ImGui** – included as `external/imgui-1.92.8/` (no separate installation required)

### Windows
- **MinGW-w64** (GCC for Windows) with `g++` compiler
- **GLFW**, **GLM**, and **GLAD** libraries set up in `C:\OpenGL\`
- **GLFW3.DLL** – copy `C:\OpenGL\glfw\lib-mingw-w64\glfw3.dll` to `bin\` after compilation
- **Note:** DICOM conversion on Windows is currently not functional (the application can still visualize pre-converted `.raw` files). Use Linux/WSL for conversion, or run the conversion script manually from a WSL terminal.

### Linux / WSL
- **GCC** compiler (`g++`) and build tools
- **GLFW** development libraries: `sudo apt install libglfw3-dev`
- **GLM** development libraries: `sudo apt install libglm-dev`
- **GLAD** – source files should be in your OpenGL directory (e.g., `/mnt/c/OpenGL/glad/`)

---

## Dataset Management

The application scans the `data/` folder for subdirectories containing `.dcm` files. Each subdirectory is presented as a selectable dataset in the graphical menu.

- **Conversion is on-demand:** When you select a dataset that hasn't been converted yet, the application calls `utils/convert_to_raw.py` with the appropriate `--input-dir` and `--output-prefix` arguments.
- **Cached conversion:** Once a dataset is converted (`.raw` + `.txt` files exist), subsequent loads are instantaneous.
- **Physical spacing preserved:** The conversion script reads `PixelSpacing` and `SliceThickness` from the DICOM metadata and stores them in the `.txt` file, ensuring correct aspect ratio even for non-cubic voxels.

The graphical interface is built with **Dear ImGui** and supports both mouse clicks and keyboard navigation:
- **Up/Down arrows** – navigate the list
- **Enter** – load/convert the selected dataset
- **H** – open the help overlay
- **Esc** – return to the menu (if in visualization) or prompt exit confirmation (if in menu)

---

## Quick Start

### Linux / WSL

```bash
make
./bin/app
```

This compiles the program and creates a Python virtual environment (`.venv/`) with `pydicom` and `numpy` installed. The executable is placed in `bin/app`.

### Windows

```bash
make -f Makefile.win
copy C:\OpenGL\glfw\lib-mingw-w64\glfw3.dll bin\
bin\app.exe
```

The Windows executable (`bin/app.exe`) requires `glfw3.dll` to be in the same folder. Conversion of DICOM files is not available on Windows; use the Linux/WSL build for conversion, or manually convert datasets using the Python script.

---

## Compilation Details

### Linux / WSL (Makefile)
- **Compiler:** `g++ -std=c++17`
- **Includes:** GLAD, GLM, Dear ImGui
- **Libraries:** GLFW, GL, dl, m
- **Sources:** all `src/*.cpp`, `glad.c`, and ImGui core + GLFW/OpenGL3 backends

### Windows (Makefile.win)
- **Compiler:** `x86_64-w64-mingw32-g++` (MinGW-w64)
- **Static linking:** `-static-libgcc -static-libstdc++`
- **Libraries:** `glfw3dll`, `opengl32`, `gdi32`
- **Sources:** same as Linux, plus ImGui backends

---

## Controls

| Key | Action |
|-----|--------|
| **D** | Toggle debug console output |
| **F** | Toggle wireframe box |
| **Q/W** | Decrease/Increase threshold (Shift: fine adjustment ±0.001) |
| **A/S** | Decrease/Increase density (Shift: fine adjustment ±0.001) |
| **Z/X** | Decrease/Increase brightness |
| **H** | Show help overlay (all controls) |
| **F11** | Toggle fullscreen |
| **ESC** | Return to dataset menu (visualization) or prompt exit (menu) |
| **Mouse Drag** | Rotate view (only when not interacting with UI) |
| **Mouse Wheel** | Zoom in/out |

---

## Manual Conversion (Advanced)

If you need to convert a dataset manually (e.g., for debugging or to run it on Windows), you can invoke the script directly:

```bash
# Convert a specific dataset (e.g., data/ct_brain/)
python utils/convert_to_raw.py --input-dir data/ct_brain --output-prefix data/ct_brain
```

The script reads all `.dcm` files in the input directory, sorts them by `InstanceNumber`, normalizes intensities to `[0,1]`, and writes:
- `data/<name>.raw` – binary float volume
- `data/<name>.txt` – dimensions and voxel spacing in mm (`width height depth spacingW spacingH spacingD`)

The orientation is fixed (transposed for OpenGL) and does not require auto-detection, ensuring consistency across datasets.

---

## Debugging

The application generates debug output files in the project root:
- **`debug_volume.txt`** – detailed volume loading information (dimensions, data statistics, non-zero voxel analysis, spacing)
- **`debug.txt`** – runtime debug output (when debug mode is enabled with **D**)

Press **D** while the app is running to toggle console debug output, showing:
- Volume dimensions and physical spacing
- Cube scale (normalized physical size)
- Camera position, yaw, pitch
- Current rendering parameters (threshold, density, brightness)

---

## Project Structure

```
.
├── bin/                    # Compiled executables
├── data/                   # DICOM datasets (subdirectories) + .raw/.txt cache
├── external/               # Third-party libraries (Dear ImGui)
├── shaders/                # GLSL vertex/fragment shaders
├── src/                    # C++ source code
│   ├── dataset_manager.cpp/h   # Dataset discovery and conversion
│   ├── debug_logger.cpp/h      # File-based logging
│   ├── main.cpp                # Entry point, ImGui integration
│   ├── renderer.cpp/h          # Volume rendering, camera, shaders
│   └── volume_loader.cpp/h     # RAW volume loading
├── utils/                  # Python scripts (convert_to_raw.py, debug)
├── Makefile                # Linux/WSL build
├── Makefile.win            # Windows (MinGW) build
└── README.md
```

---

## Third-Party Libraries

This project uses the following open-source libraries:

- **GLFW** – Window and context management  
  License: [zlib/libpng](https://www.glfw.org/license.html)  
  Copyright (c) 2002-2006 Marcus Geelnard, 2006-2019 Camilla Löwy

- **GLM** – OpenGL Mathematics  
  License: [MIT](https://github.com/g-truc/glm/blob/master/copying.txt)  
  Copyright (c) 2005 – G-Truc Creation

- **GLAD** – OpenGL loader  
  License: [MIT](https://github.com/Dav1dde/glad/blob/master/LICENSE) (or Public Domain)  
  Copyright (c) 2013-2022 David Herberth

- **Dear ImGui** – Graphical user interface  
  License: [MIT](https://github.com/ocornut/imgui/blob/master/LICENSE.txt)  
  Copyright (c) 2014-2023 Omar Cornut

Full license texts are available in the `LICENSE.txt` file included in this repository.

---

## Notes

- **Multi-dataset support:** The graphical menu automatically detects subdirectories with `.dcm` files and lets you switch between them at any time.
- **Physical spacing:** The renderer uses actual millimeter spacing (read from DICOM) to scale the volume, preventing distortion for datasets with anisotropic voxels.
- **Performance:** The shader uses 384 steps per ray with exact slab intersection, providing ~60 FPS on modern hardware. The conversion process is CPU-bound and may take a few seconds for large datasets.
- **Windows limitations:** DICOM conversion is not functional on Windows. Use Linux/WSL for conversion, or manually run the conversion script from a WSL terminal.

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| **Missing GLFW3.DLL** | Copy the DLL from `C:/OpenGL/glfw/lib-mingw-w64/` to `bin/` |
| **Conversion fails on Windows** | Run conversion from WSL or manually convert using `utils/convert_to_raw.py` |
| **Black screen / no volume** | Check `debug_volume.txt` for errors; ensure `.raw` and `.txt` files exist in `data/` |
| **ImGui not rendering** | Ensure you didn't override ImGui's GLFW callbacks (use polling, not custom callbacks) |
| **Mouse drag doesn't rotate** | Make sure you're not clicking on the ImGui menu (the drag only works outside the UI area) |
```
