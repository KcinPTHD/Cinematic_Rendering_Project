# Volume Renderer - Build & Run Guide

## Prerequisites

### Common (both platforms)
- **Git** (optional, for cloning)
- **Python 3** with `pydicom` and `numpy` installed (for DICOM conversion)
- **GLM** math library (for C++)

### Windows
- **MinGW-w64** (GCC for Windows) with `g++` compiler
- **GLFW**, **GLM**, and **GLAD** libraries set up in `C:\OpenGL\`
- **GLFW3.DLL** – copy `C:\OpenGL\glfw\lib-mingw-w64\glfw3.dll` to `bin\` after compilation

### Linux / WSL
- **GCC** compiler (`g++`) and build tools
- **GLFW** development libraries: `sudo apt install libglfw3-dev`
- **GLM** development libraries: `sudo apt install libglm-dev`
- **GLAD** – source files should be in your OpenGL directory (e.g., `/mnt/c/OpenGL/glad/`)

---

## Step 1: Convert DICOM to RAW format

**Automatic conversion (recommended):**  
The provided Makefile handles this step for you. Simply run:

- **Linux/WSL:** `make`
- **Windows:** `make -f Makefile.win`

This will create a Python virtual environment, install dependencies, and run `convert_to_raw.py --auto-detect` automatically.

---

**Manual conversion (optional):**  
If you prefer to run the conversion separately, use:

```bash
python utils/convert_to_raw.py --auto-detect
```

### What the script does:
- Reads all `.dcm` files from `data/ct/`
- Sorts them by InstanceNumber
- Normalizes intensity values to [0, 1]
- Detects optimal axis orientation (if `--auto-detect`):
  - Primary: Checks DICOM metadata (`ImageOrientationPatient` tag)
  - Fallback: Analyzes bilateral symmetry
  - Default: Uses WDH if detection is uncertain
- Outputs `data/ct.raw` and `data/ct.txt` (metadata)
- Prints debug info including detected orientation and data statistics

---

## Step 2: Compile the C++ code

### Windows (native .exe)

#### Option A: Using the provided Makefile (recommended)
The project includes `Makefile.win` for MinGW-w64.

```bash
make -f Makefile.win
```

This produces `bin/app.exe`.

**After compilation, copy the GLFW DLL:**
```bash
copy C:\OpenGL\glfw\lib-mingw-w64\glfw3.dll bin\
```

#### Option B: Manual g++ command
```bash
g++ -g -Wall -static-libgcc -static-libstdc++ src/*.cpp C:/OpenGL/glad/src/glad.c -o bin/app.exe -I C:/OpenGL/glfw/include -I C:/OpenGL/glm/include -I C:/OpenGL/glad/include -L C:/OpenGL/glfw/lib-mingw-w64 -lglfw3dll -lopengl32 -lgdi32
```

Then copy `glfw3.dll` to `bin\` as above.

---

### Linux / WSL (native binary)

#### Option A: Using the provided Makefile (recommended)
```bash
make
```

This produces `bin/app` (Linux executable).

#### Option B: Manual g++ command
```bash
g++ -g -Wall src/*.cpp /path/to/glad/src/glad.c -o bin/app -I /usr/include/glm -I /path/to/glad/include -lglfw -lGL -ldl -lm
```

*(Adjust paths to your GLAD and GLM installations.)*

---

## Step 3: Run the application

### Windows
```powershell
.\bin\app.exe
```
or with cinematic mode:
```powershell
.\bin\app.exe --cinematic
```

**Important:** The file `glfw3.dll` must be in the same folder as `app.exe` (i.e., `bin/`).

---

### Linux / WSL
```bash
./bin/app
```
or with cinematic mode:
```bash
./bin/app --cinematic
```

---

## Controls

| Key | Action |
|-----|--------|
| **D** | Toggle debug output (console + rendering stats) |
| **F** | Toggle wireframe box visibility |
| **Q/W** | Decrease/Increase threshold (hold Shift for fine control: ±0.001) |
| **A/S** | Decrease/Increase density (hold Shift for fine control: ±0.001) |
| **Z/X** | Decrease/Increase brightness |
| **Mouse Drag** | Rotate view |
| **Mouse Wheel** | Zoom in/out |

---

## Debugging

The application generates debug output files in the project root:
- **`debug_volume.txt`** – Volume loading information (dimensions, data stats, non-zero voxel analysis)
- **`debug.txt`** – Runtime debug output (when debug mode is enabled with **D** key)

Press **D** while the app is running to toggle console debug output showing:
- Current volume dimensions
- Cube coordinate ranges (centered at origin)
- Camera position and angles
- Current rendering parameters (threshold, density, brightness)

---

## Orientation Handling

**Automatic (Recommended):**
```bash
python utils/convert_to_raw.py --auto-detect
```

The script will automatically detect the correct orientation by:
1. Checking DICOM `ImageOrientationPatient` metadata
2. Analyzing bilateral symmetry to identify axis ordering
3. Printing the detected orientation in the output

---

**Manual Testing (for debugging or comparing orientations):**
```bash
cd utils
python convert_to_raw_debug.py
```

This generates 4 test files with different orientations:
- `ct_test_WHD.raw` – width, height, depth
- `ct_test_WDH.raw` – width, depth, height
- `ct_test_HWD.raw` – height, width, depth
- `ct_test_DHW.raw` – depth, height, width

Then copy the desired orientation files to `data/ct.raw` and `data/ct.txt`.

---

## Notes

- The **Makefile** (`Makefile` for Linux/WSL, `Makefile.win` for Windows) handles automatic DICOM conversion and compilation in one step.
- For Windows, remember to **copy `glfw3.dll`** to the `bin/` folder after compilation.
- If using WSL, the Linux binary runs under WSLg and will display the OpenGL window natively on Windows.
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