import nibabel as nib
import numpy as np
import sys

input_path = sys.argv[1]
output_path = sys.argv[2]

img = nib.load(input_path)
data = img.get_fdata()

print("Shape:", data.shape)

# normalizar
data = data - data.min()
data = data / data.max()
data = (data * 255).astype(np.uint8)

# corrigir ordem dos eixos: (Z,Y,X) -> (X,Y,Z)
data = np.transpose(data, (2, 1, 0))

data.tofile(output_path)

print("Saved:", output_path)