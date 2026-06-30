import os
import pydicom
import numpy as np
import argparse

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ============================================================
# MAIN PROCESSING
# ============================================================
#
# NOTA SOBRE EIXOS / ORIENTAÇÃO (lê isto antes de tocar no transpose!)
#
# O array "volume" é construído com shape (d, h, w):
#   d = número de slices (eixo craniocaudal do corpo, cabeça-pés)
#   h = altura de cada imagem (pixel_array.shape[0])
#   w = largura de cada imagem (pixel_array.shape[1])
#
# O numpy guarda arrays em C-order: o ÚLTIMO eixo da shape é o que varia
# mais rápido na memória. O glTexImage3D(width, height, depth, ...) do
# OpenGL espera precisamente o oposto: "width" deve ser o eixo mais
# rápido na memória, "depth" o mais lento.
#
# Por isso fazemos transpose para (h, d, w):
#   - w (mais rápido)  -> corresponde a glTexImage3D width
#   - d (eixo do meio) -> corresponde a glTexImage3D height
#   - h (mais lento)   -> corresponde a glTexImage3D depth
#
# Como bónus, isto faz com que o eixo do corpo (d, nº de slices) seja
# mapeado para a altura (Y) no ecrã, que é o que se quer ao visualizar
# um torso "de pé". Se um dia precisares de outra orientação, NÃO
# inventes outro transpose ad-hoc — ajusta antes a câmara/yaw/pitch no
# renderer, ou mantém este transpose fixo e gira o objeto no C++.
#
# O ficheiro .txt é escrito exatamente na ordem (w, d, h), que é a
# mesma ordem que o renderer.cpp passa ao glTexImage3D(width,height,depth).

def main():
    parser = argparse.ArgumentParser(description='Convert DICOM to RAW volume.')
    parser.add_argument('--input-dir', type=str, default='data/ct',
                        help='Directory containing DICOM files (default: data/ct)')
    parser.add_argument('--output-prefix', type=str, default='data/ct',
                        help='Output file prefix (e.g., data/ct -> data/ct.raw and data/ct.txt)')
    args = parser.parse_args()

    folder = os.path.join(BASE, args.input_dir)
    output_raw = args.output_prefix + '.raw'
    output_meta = args.output_prefix + '.txt'

    slices = []

    for file in os.listdir(folder):
        if file.endswith(".dcm"):
            path = os.path.join(folder, file)
            dcm = pydicom.dcmread(path)
            slices.append(dcm)

    slices.sort(key=lambda s: int(s.InstanceNumber))

    print(f"[INFO] Loaded {len(slices)} DICOM files")

    h, w = slices[0].pixel_array.shape
    d = len(slices)

    print(f"[INFO] Slice dimensions: {w}x{h}, Total slices: {d}")

    # -----------------------------
    # PHYSICAL VOXEL SPACING (mm)
    # -----------------------------
    # Os voxels quase nunca são cúbicos num scan real: o espaçamento
    # entre pixels dentro de um corte (PixelSpacing) é normalmente muito
    # mais fino do que a espessura do corte (SliceThickness). Se
    # ignorarmos isto e escalarmos só pela CONTAGEM de voxels (ex.
    # 512x512x150), um scan de tórax fica "esmagado" no eixo dos cortes,
    # porque 150 cortes de 4mm cobrem muito menos distância proporcional
    # do que 512 pixels de 0.7mm.
    #
    # PixelSpacing (DICOM) = [row_spacing, col_spacing] em mm:
    #   row_spacing -> espaçamento ao longo do eixo "h" (linhas)
    #   col_spacing -> espaçamento ao longo do eixo "w" (colunas)
    ref = slices[0]
    pixel_spacing = getattr(ref, "PixelSpacing", [1.0, 1.0])
    row_spacing = float(pixel_spacing[0])  # eixo h
    col_spacing = float(pixel_spacing[1])  # eixo w

    # Espessura/espaçamento entre cortes -> eixo "d"
    if hasattr(ref, "SpacingBetweenSlices"):
        slice_spacing = float(ref.SpacingBetweenSlices)
    elif hasattr(ref, "SliceThickness"):
        slice_spacing = float(ref.SliceThickness)
    else:
        slice_spacing = 1.0

    print(f"[INFO] Voxel spacing (mm) -> w:{col_spacing} d:{slice_spacing} h:{row_spacing}")

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

    # Transpose fixo (d,h,w) -> (h,d,w). Ver explicação grande no topo do
    # ficheiro. w fica o eixo mais rápido na memória (-> GL width),
    # d o eixo do meio (-> GL height), h o mais lento (-> GL depth).
    volume = np.transpose(volume, (1, 0, 2))

    print(f"[INFO] Final array shape (H,D,W) in memory: {volume.shape}")
    print(f"[INFO] GL texture dims -> width(w)={w} height(d)={d} depth(h)={h}")

    volume.tofile(output_raw)

    print(f"[OK] Saved RAW: {output_raw}")
    print(f"[OK] Size: {volume.nbytes / (1024*1024):.2f} MB")

    # Gravado na ordem (w, d, h) para as contagens de voxels, seguido do
    # espaçamento físico em mm na MESMA ordem de eixos (w, d, h). O
    # renderer usa contagem*espaçamento para calcular o tamanho físico
    # real de cada eixo, em vez de assumir voxels cúbicos.
    with open(output_meta, "w") as f:
        f.write(f"{w} {d} {h} {col_spacing} {slice_spacing} {row_spacing}")

    print(f"[OK] Saved META: {output_meta}")
    print(f"[OK] GL dims order in .txt -> width({w}) height({d}) depth({h})")
    print(f"[OK] Spacing order in .txt -> width({col_spacing}) height({slice_spacing}) depth({row_spacing}) mm")

if __name__ == "__main__":
    main()
