import os
import sys
import pydicom
import numpy as np

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ============================================================
# USAGE
# ============================================================
# python convert_to_raw.py              → Uses default WDH orientation
# python convert_to_raw.py --auto-detect → Auto-detects best orientation
# ============================================================

# ============================================================
# AUTO-ORIENTATION DETECTION
# ============================================================

def parse_dicom_orientation(dicom_file):
    """
    Parse ImageOrientationPatient from DICOM to determine axis ordering.
    Returns orientation string (WHD, WDH, HWD, DHW) or None if not available.
    """
    try:
        orientation = dicom_file.get('ImageOrientationPatient', None)
        if orientation is None:
            return None
        
        # ImageOrientationPatient: [X_row, Y_row, Z_row, X_col, Y_col, Z_col]
        # direction cosines for row and column axes
        
        # Standard medical orientation (most common):
        # (1,0,0,0,1,0) = standard axial (X-row, Y-col) → WHD
        # We'd need to check multiple DICOM files to establish a pattern
        
        print(f"[DEBUG] ImageOrientationPatient: {orientation}")
        
        # Standard axial (head scans) uses (1,0,0,0,1,0) or close to it
        if abs(orientation[0] - 1.0) < 0.1 and abs(orientation[4] - 1.0) < 0.1:
            return "WDH"  # Standard axial
        
        return None
    except Exception as e:
        print(f"[DEBUG] Could not parse orientation: {e}")
        return None


def bilateral_symmetry(volume, axis):
    """
    Calculate bilateral symmetry correlation along a given axis.
    Returns correlation coefficient [-1, 1]. Higher = more symmetric.
    """
    try:
        mid = volume.shape[axis] // 2
        left = np.take(volume, range(mid), axis=axis)
        right = np.take(volume, range(mid, min(2*mid, volume.shape[axis])), axis=axis)
        
        # Align shapes if needed
        min_len = min(left.shape[axis], right.shape[axis])
        left = np.take(left, range(min_len), axis=axis)
        right = np.take(right, range(min_len), axis=axis)
        right_flipped = np.flip(right, axis=axis)
        
        # Flatten and compute correlation
        left_flat = left.flatten()
        right_flat = right_flipped.flatten()
        
        if len(left_flat) > 0 and np.std(left_flat) > 0 and np.std(right_flat) > 0:
            correlation = np.corrcoef(left_flat, right_flat)[0, 1]
            if np.isnan(correlation):
                return -1.0
            return correlation
        return -1.0
    except Exception as e:
        print(f"[DEBUG] Symmetry calculation failed for axis {axis}: {e}")
        return -1.0


def detect_best_orientation(volume):
    """
    Detect best orientation by testing bilateral symmetry on all 3 axes.
    Returns orientation string (WHD, WDH, HWD, DHW).
    """
    symmetries = {}
    
    # Map axes to potential orientations
    # The axis with highest symmetry is likely the left-right axis
    # For head scans: typically width or height is left-right
    
    for axis in range(3):
        sym = bilateral_symmetry(volume, axis)
        symmetries[axis] = sym
        print(f"[DEBUG] Axis {axis} bilateral symmetry: {sym:.4f}")
    
    # Find axis with highest symmetry
    best_axis = max(symmetries, key=symmetries.get)
    best_sym = symmetries[best_axis]
    
    print(f"[INFO] Best symmetric axis: {best_axis} (score: {best_sym:.4f})")
    
    # Map axis to likely orientation
    # (This is a heuristic; can be refined with more datasets)
    if best_sym > 0.3:  # Threshold for "symmetric enough"
        if best_axis == 0:
            return "WDH"  # Width axis is symmetric (left-right)
        elif best_axis == 1:
            return "WHD"  # Height axis is symmetric (left-right)
        else:
            return "WDH"  # Default if depth is symmetric
    
    return "WDH"  # Final fallback


def auto_detect_orientation(slices_list):
    """
    Auto-detect orientation from DICOM slices.
    Strategy:
    1. Check DICOM metadata (ImageOrientationPatient)
    2. Fall back to bilateral symmetry detection
    3. Default to WDH
    Returns orientation string.
    """
    print("[INFO] Auto-detecting orientation...")
    
    # Try metadata first
    if len(slices_list) > 0:
        metadata_result = parse_dicom_orientation(slices_list[0])
        if metadata_result:
            print(f"[OK] Detected from metadata: {metadata_result}")
            return metadata_result
    
    # Build test volume for symmetry analysis
    h, w = slices_list[0].pixel_array.shape
    d = len(slices_list)
    
    print(f"[INFO] Building test volume ({w}x{h}x{d}) for symmetry analysis...")
    test_volume = np.zeros((d, h, w), dtype=np.float32)
    
    for i, s in enumerate(slices_list):
        img = s.pixel_array.astype(np.float32)
        if hasattr(s, "RescaleSlope") and hasattr(s, "RescaleIntercept"):
            img = img * s.RescaleSlope + s.RescaleIntercept
        test_volume[i] = img
    
    # Normalize for analysis
    test_volume -= test_volume.min()
    vol_max = test_volume.max()
    if vol_max > 0:
        test_volume /= vol_max
    
    # Transpose to (width, height, depth) for analysis
    test_volume = np.transpose(test_volume, (2, 1, 0))
    
    # Detect based on symmetry
    result = detect_best_orientation(test_volume)
    print(f"[OK] Detected from bilateral symmetry: {result}")
    
    return result


# ============================================================
# MAIN PROCESSING
# ============================================================

folder = os.path.join(BASE, "data", "ct")
output = os.path.join(BASE, "data", "ct.raw")

slices = []

# carregar todos os dcm
for file in os.listdir(folder):
    if file.endswith(".dcm"):
        path = os.path.join(folder, file)
        dcm = pydicom.dcmread(path)
        slices.append(dcm)

# ORDENAR (crucial!)
slices.sort(key=lambda s: int(s.InstanceNumber))

print(f"[INFO] Loaded {len(slices)} DICOM files")

# Check DICOM orientation (for debugging)
first_slice = slices[0]
print(f"[DEBUG] First slice orientation: {first_slice.get('PatientOrientation', 'N/A')}")
print(f"[DEBUG] Slice location: {first_slice.get('SliceLocation', 'N/A')}")

# dimensões
h, w = slices[0].pixel_array.shape
d = len(slices)

print(f"[INFO] Slice dimensions: {w}x{h}, Total slices: {d}")

volume = np.zeros((d, h, w), dtype=np.float32)

for i, s in enumerate(slices):
    img = s.pixel_array.astype(np.float32)

    # CT → Hounsfield normalization (opcional mas recomendado)
    if hasattr(s, "RescaleSlope") and hasattr(s, "RescaleIntercept"):
        img = img * s.RescaleSlope + s.RescaleIntercept

    volume[i] = img

print(f"[DEBUG] Before normalization - Min: {volume.min()}, Max: {volume.max()}, Mean: {volume.mean()}")

# NORMALIZAR 0..1
volume -= volume.min()
vol_max = volume.max()
if vol_max > 0:
    volume /= vol_max

print(f"[DEBUG] After normalization - Min: {volume.min()}, Max: {volume.max()}, Mean: {volume.mean()}")

# AUTO-DETECT ORIENTATION if requested
auto_detect = "--auto-detect" in sys.argv
if auto_detect:
    orientation = auto_detect_orientation(slices)
    print(f"[INFO] Using auto-detected orientation: {orientation}")
else:
    orientation = "WDH"  # Default
    print(f"[INFO] Using default orientation: {orientation}")

# Reorder to OpenGL (X,Y,Z) based on detected orientation
# volume is currently (D, H, W) from stacking slices
# We need to transpose based on orientation

if orientation == "WDH":
    # width, depth, height → (2, 0, 1)
    volume = np.transpose(volume, (2, 0, 1))
elif orientation == "WHD":
    # width, height, depth → (2, 1, 0)
    volume = np.transpose(volume, (2, 1, 0))
elif orientation == "HWD":
    # height, width, depth → (1, 2, 0)
    volume = np.transpose(volume, (1, 2, 0))
elif orientation == "DHW":
    # depth, height, width → (0, 1, 2)
    volume = np.transpose(volume, (0, 1, 2))
else:
    # Fallback to WDH
    volume = np.transpose(volume, (2, 0, 1))
    orientation = "WDH"

print(f"[INFO] Final volume shape (X,Y,Z): {volume.shape}")

volume.tofile(output)

print(f"[OK] Saved RAW: {output}")
print(f"[OK] Size: {volume.nbytes / (1024*1024):.2f} MB")
print(f"[OK] Orientation: {orientation}")

meta_path = os.path.join(os.path.dirname(output), "ct.txt")

with open(meta_path, "w") as f:
    f.write(f"{volume.shape[0]} {volume.shape[1]} {volume.shape[2]}")

print(f"[OK] Saved META: {meta_path}")
print(f"[OK] Format: Width({volume.shape[0]}) Depth({volume.shape[1]}) Height({volume.shape[2]})")