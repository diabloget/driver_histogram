import os, numpy as np, pandas as pd

BASE = "."
GRAY_CSV  = os.path.join(BASE,"gray_matrix.csv")
EDGES_CSV = os.path.join(BASE,"edges_matrix.csv")
HIST_CSV  = os.path.join(BASE,"histograma.csv")
GRAY_BIN  = os.path.join(BASE,"gray_matrix.bin")
EDGES_BIN = os.path.join(BASE,"edges_matrix.bin")

def ok(b): return "OK" if b else "FAIL"

def load_csv(p):
    return np.loadtxt(p, delimiter=',', dtype=np.uint16)

def load_bin(p, shape):
    H,W = shape
    n = H*W
    with open(p,"rb") as f: b = f.read()
    if len(b)!=n: return None, f"Tam. {len(b)} != {n}"
    return np.frombuffer(b, dtype=np.uint8).reshape(H,W), None

# 1) Cargar matrices
gray  = load_csv(GRAY_CSV)
edges = load_csv(EDGES_CSV)

H1,W1 = gray.shape; H2,W2 = edges.shape
print("== DIMENSIONES ==")
print(f"gray : {H1} x {W1}")
print(f"edges: {H2} x {W2}")
print("Iguales dimensiones:", ok((H1==H2 and W1==W2)))

# 2) Rango [0,255]
gmin,gmax = int(gray.min()), int(gray.max())
emin,emax = int(edges.min()), int(edges.max())
print("\n== RANGOS ==")
print(f"gray  min..max = {gmin}..{gmax} -> {ok(0<=gmin<=255 and 0<=gmax<=255)}")
print(f"edges min..max = {emin}..{emax} -> {ok(0<=emin<=255 and 0<=emax<=255)}")

# 3) Histograma debe coincidir con edges
print("\n== HISTOGRAMA ==")
hist_csv = pd.read_csv(HIST_CSV)
h = np.bincount(edges.ravel().astype(np.uint8), minlength=256)
mismatch = [(v,int(h[v]),int(c)) for v,c in zip(hist_csv["valor"], hist_csv["conteo"]) if int(h[v])!=int(c)]
print("Histograma coincide:", ok(len(mismatch)==0))
if mismatch:
    print("Primeras diferencias (valor, calculado, csv):")
    for t in mismatch[:20]: print(t)

# 4) BIN vs CSV (byte-a-byte)
print("\n== BINARIOS (≡ CSV) ==")
if os.path.exists(GRAY_BIN):
    gbin, err = load_bin(GRAY_BIN, gray.shape)
    print("gray_matrix.bin:", "OK" if (err is None and np.array_equal(gbin, gray.astype(np.uint8))) else f"FAIL ({err or 'contenido ≠ CSV'})")
else:
    print("gray_matrix.bin: no encontrado")

if os.path.exists(EDGES_BIN):
    ebin, err = load_bin(EDGES_BIN, edges.shape)
    print("edges_matrix.bin:", "OK" if (err is None and np.array_equal(ebin, edges.astype(np.uint8))) else f"FAIL ({err or 'contenido ≠ CSV'})")
else:
    print("edges_matrix.bin: no encontrado")

# 5) Chequeo de costuras (halo) aprox con 3 franjas: H/3 y 2H/3
print("\n== COSTURAS (SEAM CHECK) ==")
def row_diff(r1,r2): return float(np.abs(edges[r1,:].astype(np.int16)-edges[r2,:].astype(np.int16)).mean())
H,W = edges.shape
adj = [row_diff(r,r+1) for r in range(H-1)]
gmean, gstd = float(np.mean(adj)), float(np.std(adj))
print(f"Δ fila ady. global: mean={gmean:.3f}, σ={gstd:.3f}")
for c in [H//3, (2*H)//3]:
    if 1 <= c < H-1:
        d = row_diff(c-1,c)
        z = (d-gmean)/(gstd+1e-9)
        print(f"Seam {c-1}/{c}: Δ={d:.3f}, z≈{z:.2f} ->", "OK" if abs(z)<=3.0 else "SOSPECHOSO")

print("\n[FIN] Si todo marca OK, tu flujo 4.4 está coherente.")
