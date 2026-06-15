## Volume Renderer - Build & Run Guide

### Prerequisites
- MinGW/GCC installed with g++ compiler
- GLFW, GLM, and GLAD libraries already set up in `C:\OpenGL\`
- Python 3 with pydicom and numpy installed (for DICOM conversion)

### Step 1: Convert DICOM to RAW format

**Default (uses WDH orientation):**
```bash
python utils/convert_to_raw.py
```

**Auto-detect best orientation:**
```bash
python utils/convert_to_raw.py --auto-detect
```

This script:
- Reads all `.dcm` files from `data/ct/`
- Sorts them by InstanceNumber
- Normalizes intensity values to [0, 1]
- Detects optimal axis orientation (if `--auto-detect` flag is used):
  - Primary: Checks DICOM metadata (ImageOrientationPatient tag)
  - Fallback: Analyzes bilateral symmetry to find best orientation
  - Default: Uses WDH if detection is uncertain
- Outputs `data/ct.raw` and `data/ct.txt` (metadata)
- Prints debug information including detected orientation and data statistics

### Step 2: Compile the C++ code

**Using g++ directly:**
```bash
g++ -g src/*.cpp C:/OpenGL/glad/src/glad.c -o bin/app.exe -I C:/OpenGL/glfw/include -I C:/OpenGL/glm/include -I C:/OpenGL/glad/include -L C:/OpenGL/glfw/lib-mingw-w64 -lglfw3dll -lopengl32
```

**Or use the VS Code build task (Ctrl+Shift+B)**

### Step 3: Run the application

```bash
bin/app.exe
```

### Controls

| Key | Action |
|-----|--------|
| **D** | Toggle debug output (console + rendering stats) |
| **F** | Toggle wireframe box visibility |
| **Q/W** | Decrease/Increase threshold (hold Shift for fine control: ±0.001) |
| **A/S** | Decrease/Increase density (hold Shift for fine control: ±0.001) |
| **Z/X** | Decrease/Increase brightness |
| **Mouse Drag** | Rotate view |
| **Mouse Wheel** | Zoom in/out |

### Debugging

The application generates debug output:
- **debug_volume.txt** - Volume loading information (dimensions, data stats, non-zero voxel analysis)
- **debug.txt** - Runtime debug output (when debug mode is enabled with D key)

Press **D** while the app is running to toggle debug console output showing:
- Current volume dimensions
- Cube coordinate ranges (now centered at origin)
- Camera position and angles
- Current rendering parameters (threshold, density, brightness)

### Recent Fixes

1. **Centered Cube Geometry** - Cube now centered at origin for proper ray-casting alignment
2. **Coordinate System** - Texture coordinates properly mapped from centered world space to [0,1]
3. **DICOM Processing** - Enhanced debugging output to verify slice ordering and data normalization
4. **Shader Math** - Fixed ray-volume intersection calculations for centered geometry
5. **Precise Controls** - Added Shift modifier for fine-grained threshold/density adjustment (±0.001 vs ±0.01)
6. **Z-Fighting Eliminated** - Wireframe now rendered with depth test disabled to prevent artifacts
7. **Wireframe Toggle** - Press F to show/hide wireframe reference box

### Orientation Handling

**Automatic (Recommended):**
```bash
python utils/convert_to_raw.py --auto-detect
```
The script will automatically detect the correct orientation by:
1. Checking DICOM ImageOrientationPatient metadata
2. Analyzing bilateral symmetry to identify axis ordering
3. Printing the detected orientation in the output

**Manual Testing (for debugging or comparing orientations):**
```bash
cd utils
python convert_to_raw_debug.py
```
This generates 4 test files with different orientations:
- `ct_test_WHD.raw` - width, height, depth
- `ct_test_WDH.raw` - width, depth, height
- `ct_test_HWD.raw` - height, width, depth
- `ct_test_DHW.raw` - depth, height, width

Then copy the desired orientation files to `data/ct.raw` and `data/ct.txt`.

