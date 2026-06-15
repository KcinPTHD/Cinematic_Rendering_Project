import os
import pydicom
import numpy as np

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

folder = os.path.join(BASE, "data", "ct")
output_base = os.path.join(BASE, "data", "ct_test")

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

# dimensões
h, w = slices[0].pixel_array.shape
d = len(slices)

print(f"[INFO] Slice dimensions: {w}x{h}, Total slices: {d}")

volume = np.zeros((d, h, w), dtype=np.float32)

for i, s in enumerate(slices):
    img = s.pixel_array.astype(np.float32)

    if hasattr(s, "RescaleSlope") and hasattr(s, "RescaleIntercept"):
        img = img * s.RescaleSlope + s.RescaleIntercept

    volume[i] = img

print(f"[DEBUG] Before normalization - Min: {volume.min()}, Max: {volume.max()}, Mean: {volume.mean()}")

volume -= volume.min()
vol_max = volume.max()
if vol_max > 0:
    volume /= vol_max

print(f"[DEBUG] After normalization - Min: {volume.min()}, Max: {volume.max()}, Mean: {volume.mean()}")

# TEST DIFFERENT AXIS ORDERS
test_orders = [
    ((2, 1, 0), "WHD (default: width, height, depth)"),
    ((2, 0, 1), "WDH (width, depth, height)"),
    ((1, 2, 0), "HWD (height, width, depth)"),
    ((0, 2, 1), "DHW (depth, height, width)"),
]

for order, desc in test_orders:
    vol_test = np.transpose(volume, order)
    print(f"\n[TEST] Order {order} ({desc}): shape {vol_test.shape}")
    
    output = f"{output_base}_{desc.split()[0]}.raw"
    output_meta = f"{output_base}_{desc.split()[0]}.txt"
    
    vol_test.tofile(output)
    
    with open(output_meta, "w") as f:
        f.write(f"{vol_test.shape[0]} {vol_test.shape[1]} {vol_test.shape[2]}")
    
    print(f"  Saved: {output}")

print("\n[INFO] Test files created. Copy desired one to ct.raw and update ct.txt")
