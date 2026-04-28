import os
import pydicom
import numpy as np

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

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

# dimensões
h, w = slices[0].pixel_array.shape
d = len(slices)

volume = np.zeros((d, h, w), dtype=np.float32)

for i, s in enumerate(slices):
    img = s.pixel_array.astype(np.float32)

    # CT → Hounsfield normalization (opcional mas recomendado)
    if hasattr(s, "RescaleSlope") and hasattr(s, "RescaleIntercept"):
        img = img * s.RescaleSlope + s.RescaleIntercept

    volume[i] = img

# NORMALIZAR 0..1
volume -= volume.min()
volume /= volume.max()

# reorder para OpenGL (X,Y,Z)
volume = np.transpose(volume, (2,1,0))

volume.tofile(output)

print("Saved RAW:", output)
print("Dims:", volume.shape)

meta_path = os.path.join(os.path.dirname(output), "ct.txt")

with open(meta_path, "w") as f:
    f.write(f"{volume.shape[0]} {volume.shape[1]} {volume.shape[2]}")

print("Saved META:", meta_path)