#!/usr/bin/env python3
"""
validar_sobel_pipeline.py

Script para validar el pipeline:
    imagen -> Sobel (Gx, Gy) -> |Gx| + |Gy| -> histograma

Permite comparar contra:
  - sobel_full.txt generado por el cluster (opcional)
  - histogram_sobel.csv generado por el cluster (opcional)

Uso típico:
    python validar_sobel_pipeline.py
y luego seguir las instrucciones en consola.
"""

import sys
import csv
from pathlib import Path

import numpy as np
from PIL import Image


# ------------------------------------------------------------
# 1. Lectura de la imagen y conversión a escala de grises
# ------------------------------------------------------------

def cargar_imagen_gris(path_str: str) -> np.ndarray:
    """
    Carga una imagen y la convierte a escala de grises (uint8).
    Devuelve un array numpy HxW de tipo uint8.
    """
    path = Path(path_str)
    if not path.is_file():
        raise FileNotFoundError(f"No se encontró la imagen: {path}")
    img = Image.open(path).convert("L")  # L = 8-bit grayscale
    gray = np.array(img, dtype=np.uint8)
    return gray


# ------------------------------------------------------------
# 2. Parseo de máscara Sobel estilo C: x=[...] y=[...]
# ------------------------------------------------------------

def parse_kernel_9(kernel_str: str):
    """
    Parsea un string del estilo "[a,b,c,d,e,f,g,h,i]" o
    "[a b c d e f g h i]" a una lista de 9 ints.
    """
    s = kernel_str.strip()
    if not s.startswith("[") or "]" not in s:
        raise ValueError(f"Formato de kernel inválido: {s}")
    inside = s[1:s.index("]")]
    tokens = inside.replace(",", " ").split()
    if len(tokens) != 9:
        raise ValueError(f"Se esperaban 9 coeficientes y se encontraron {len(tokens)} en: {s}")
    vals = [int(t) for t in tokens]
    return vals


def parse_sobel_xy(line: str):
    """
    Parsea una línea del estilo:
       x=[-1,0,1,-2,0,2,-1,0,1] y=[1,2,1,0,0,0,-1,-2,-1]

    Devuelve:
       Kx, Ky como arrays 3x3 de ints (numpy).
    """
    lower = line.lower()
    ix = lower.find("x=")
    iy = lower.find("y=")
    if ix == -1 or iy == -1:
        raise ValueError("No se encontraron prefijos 'x=' y 'y=' en la máscara Sobel.")

    # Buscar '[' y ']' para X
    sx = line[ix+2:]  # parte después de 'x='
    bx = sx.find("[")
    ex = sx.find("]")
    if bx == -1 or ex == -1:
        raise ValueError("Falta '[' o ']' en la definición de X.")
    kx_str = sx[bx:ex+1]

    # Buscar '[' y ']' para Y
    sy = line[iy+2:]  # parte después de 'y='
    by = sy.find("[")
    ey = sy.find("]")
    if by == -1 or ey == -1:
        raise ValueError("Falta '[' o ']' en la definición de Y.")
    ky_str = sy[by:ey+1]

    kx_vals = parse_kernel_9(kx_str)
    ky_vals = parse_kernel_9(ky_str)

    Kx = np.array(kx_vals, dtype=int).reshape((3, 3))
    Ky = np.array(ky_vals, dtype=int).reshape((3, 3))
    return Kx, Ky


# ------------------------------------------------------------
# 3. Aplicar Sobel: |Gx| + |Gy|
# ------------------------------------------------------------

def aplicar_sobel_abs(gray: np.ndarray, Kx: np.ndarray, Ky: np.ndarray) -> np.ndarray:
    """
    Aplica la máscara Sobel dada por Kx, Ky sobre la imagen gris.
    Calcula: val = |Gx| + |Gy| para cada píxel interior.
    Borde exterior queda en 0.
    Devuelve un array HxW de int32.
    """
    H, W = gray.shape
    sobel = np.zeros((H, W), dtype=np.int32)

    # Recorremos solo zona interior [1..H-2], [1..W-2]
    for y in range(1, H - 1):
        for x in range(1, W - 1):
            gx = 0
            gy = 0
            for ky in range(-1, 2):
                for kx in range(-1, 2):
                    pix = int(gray[y + ky, x + kx])
                    gx += pix * int(Kx[ky + 1, kx + 1])
                    gy += pix * int(Ky[ky + 1, kx + 1])
            val = abs(gx) + abs(gy)
            sobel[y, x] = val

    return sobel


# ------------------------------------------------------------
# 4. Histograma tipo generar_histograma() en C
# ------------------------------------------------------------

def compute_hist_from_values(values):
    """
    Replica la lógica de generar_histograma() de C sobre una lista de ints.
      - Encuentra min_val y max_val.
      - range = max - min; si 0 -> todo bin 0.
      - bin = int(((val - min_val)/range) * 255.0), clamp 0..255
    Devuelve lista hist[256].
    """
    if len(values) == 0:
        raise ValueError("No hay valores para generar histograma.")

    min_val = min(values)
    max_val = max(values)

    hist = [0] * 256
    rng = float(max_val - min_val)
    if rng == 0.0:
        # caso degenerado
        hist[0] = len(values)
        return hist

    for v in values:
        # Ajuste para evitar errores de precisión flotante
        normalized = ((v - min_val) / rng) * 255.0
        bin_idx = int(round(normalized))  # Usar round para mayor precisión
        if bin_idx < 0:
            bin_idx = 0
        elif bin_idx > 255:
            bin_idx = 255
        hist[bin_idx] += 1

    return hist


# ------------------------------------------------------------
# 5. Lectura de sobel_full.txt y histograma CSV
# ------------------------------------------------------------

def read_sobel_txt(path_str: str):
    """
    Lee sobel_full.txt en formato:
      H W
      v11 v12 ... v1W
      ...
    Devuelve:
      - lista de valores enteros (flatten),
      - alto H, ancho W
    """
    path = Path(path_str)
    if not path.is_file():
        raise FileNotFoundError(f"No se encontró sobel_full.txt: {path}")

    vals = []
    with open(path, "r") as f:
        header = f.readline()
        if not header:
            raise ValueError("sobel_full.txt vacío o sin header 'H W'.")
        parts = header.strip().split()
        if len(parts) != 2:
            raise ValueError(f"Header inválido, se esperaba 'H W' y se obtuvo: {header}")
        H = int(parts[0])
        W = int(parts[1])

        for line in f:
            for tok in line.strip().split():
                try:
                    vals.append(int(tok))
                except ValueError:
                    # ignorar basura
                    continue

    if len(vals) != H * W:
        print(f"⚠️ Advertencia: se esperaban {H*W} valores, pero se leyeron {len(vals)}.")

    return vals, H, W


def read_hist_csv(path_str: str):
    """
    Lee histogram_sobel.csv con formato:
        valor,conteo
        0,123
        1,456
        ...
    Devuelve lista hist[256].
    """
    path = Path(path_str)
    if not path.is_file():
        raise FileNotFoundError(f"No se encontró histogram_sobel.csv: {path}")

    hist = [0] * 256
    with open(path, newline="") as f:
        reader = csv.reader(f)
        # Saltar encabezado si existe
        header = next(reader, None)
        for row in reader:
            if len(row) < 2:
                continue
            try:
                idx = int(row[0])
                cnt = int(row[1])
            except ValueError:
                continue
            if 0 <= idx < 256:
                hist[idx] = cnt
    return hist


# ------------------------------------------------------------
# 6. Comparaciones
# ------------------------------------------------------------

def comparar_sobel(sobel_ours: np.ndarray, vals_ref, H_ref, W_ref):
    """
    Compara nuestro resultado Sobel (sobel_ours) contra los valores
    de sobel_full.txt (vals_ref, H_ref, W_ref).
    """
    H, W = sobel_ours.shape
    sobel_flat = sobel_ours.flatten().tolist()

    print("\n=== Comparación Sobel vs sobel_full.txt ===")
    print(f"Dimensión Sobel (Python): {H}x{W}")
    print(f"Dimensión Sobel (ref)   : {H_ref}x{W_ref}")

    if H != H_ref or W != W_ref:
        print("❌ Las dimensiones no coinciden, la comparación no será exacta.")
        # Comparamos hasta el mínimo
        min_len = min(len(sobel_flat), len(vals_ref))
    else:
        min_len = len(sobel_flat)

    diffs = []
    for i in range(min_len):
        if sobel_flat[i] != vals_ref[i]:
            diffs.append((i, sobel_flat[i], vals_ref[i]))
            if len(diffs) >= 10:
                break

    if len(diffs) == 0 and H == H_ref and W == W_ref:
        print("✅ Sobel coincide EXACTAMENTE con sobel_full.txt (mismo tamaño y valores).")
    else:
        if H != H_ref or W != W_ref:
            print("⚠️ Dimensiones distintas, se comparó solo hasta el mínimo número de elementos.")
        print(f"Se encontraron diferencias en {len(diffs)} posiciones (se muestran hasta 10):")
        for idx, v_py, v_ref in diffs:
            y = idx // W
            x = idx % W
            print(f"  pos {idx} (y={y}, x={x}): py={v_py}, ref={v_ref}")


def comparar_hist(hist_ours, hist_ref):
    """
    Compara dos histogramas hist[256].
    """
    print("\n=== Comparación Histograma vs histogram_sobel.csv ===")
    total_ours = sum(hist_ours)
    total_ref = sum(hist_ref)

    print(f"Total muestras (Python): {total_ours}")
    print(f"Total muestras (CSV)   : {total_ref}")

    max_diff = 0
    diffs = []
    for i in range(256):
        d = abs(hist_ours[i] - hist_ref[i])
        if d != 0:
            diffs.append((i, hist_ours[i], hist_ref[i], d))
            if d > max_diff:
                max_diff = d
            if len(diffs) >= 10:
                # guardamos primeras 10 diferencias visibles
                pass

    if max_diff == 0 and total_ours == total_ref:
        print("✅ El histograma coincide EXACTAMENTE con histogram_sobel.csv.")
    else:
        print(f"⚠️ Hay diferencias. Máxima diferencia en un bin: {max_diff}")
        print("Primeras diferencias (bin, py, csv, |Δ|):")
        for b, hp, hc, d in diffs[:10]:
            print(f"  bin {b:3d}: py={hp} csv={hc} diff={d}")


# ------------------------------------------------------------
# 7. main interactivo
# ------------------------------------------------------------

def main():
    print("===== Validador de pipeline Sobel + Histograma =====\n")

    # 1) Imagen
    img_path = input("Ruta de la imagen (ej: ../files/foto.jpg): ").strip()
    if img_path.lower() == "no" or img_path == "":
        print("La imagen es obligatoria para realizar el pipeline. Abortando.")
        sys.exit(1)

    # 2) Máscara Sobel X/Y
    print("\nIngrese la máscara Sobel en formato:")
    print("  x=[-1,0,1,-2,0,2,-1,0,1] y=[1,2,1,0,0,0,-1,-2,-1]")
    mask_line = input("Máscara Sobel X/Y: ").strip()
    try:
        Kx, Ky = parse_sobel_xy(mask_line)
    except Exception as e:
        print(f"Error parseando la máscara Sobel: {e}")
        sys.exit(1)

    # 3) Rutas opcionales de referencia
    sobel_txt_path = input("\nRuta a sobel_full.txt (o 'no' para omitir): ").strip()
    if sobel_txt_path.lower() == "no" or sobel_txt_path == "":
        sobel_txt_path = None

    hist_csv_path = input("Ruta a histogram_sobel.csv (o 'no' para omitir): ").strip()
    if hist_csv_path.lower() == "no" or hist_csv_path == "":
        hist_csv_path = None

    # --------------------------------------------------------
    # Pipeline en Python
    # --------------------------------------------------------
    print("\n[Cargando imagen y aplicando Sobel...]")
    gray = cargar_imagen_gris(img_path)
    H, W = gray.shape
    print(f"Imagen gris: {H}x{W}")

    sobel = aplicar_sobel_abs(gray, Kx, Ky)
    sobel_flat = sobel.flatten().tolist()

    print("Calculando histograma (mismo criterio que generar_histograma)...")
    hist_py = compute_hist_from_values(sobel_flat)

    # --------------------------------------------------------
    # Comparaciones opcionales
    # --------------------------------------------------------
    if sobel_txt_path is not None:
        try:
            vals_ref, H_ref, W_ref = read_sobel_txt(sobel_txt_path)
            comparar_sobel(sobel, vals_ref, H_ref, W_ref)
        except Exception as e:
            print(f"\n❌ Error al comparar con sobel_full.txt: {e}")

    if hist_csv_path is not None:
        try:
            hist_ref = read_hist_csv(hist_csv_path)
            comparar_hist(hist_py, hist_ref)
        except Exception as e:
            print(f"\n❌ Error al comparar con histogram_sobel.csv: {e}")

    print("\nProceso finalizado.")


if __name__ == "__main__":
    main()
