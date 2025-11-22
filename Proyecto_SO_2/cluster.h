#ifndef CLUSTER_H
#define CLUSTER_H

#include <stdint.h>
#include "metrics.h"   // métricas

// ---------------------------------------------------------------------
// Funciones núcleo del clúster
// ---------------------------------------------------------------------

// Asegura que exista un directorio (crea si no existe).
void ensure_dir(const char *path);

// Parsea una máscara 3x3 escrita como "[e1, e2, ..., e9]" en K[3][3].
// Devuelve 0 en éxito, -1 en error de formato.
int parse_kernel9(const char *s, int K[3][3]);

// Carga una imagen (jpg/png/ppm/…) y la convierte a matriz gris 8 bits.
// - path: ruta al archivo de imagen
// - gray_out: puntero de salida a buffer asignado con malloc (debes free())
// - W, H: ancho y alto de la imagen
// Retorna 0 en éxito, <0 en error.
int load_any_to_gray(const char *path, uint8_t **gray_out, int *W, int *H);

// Guarda una matriz uint8_t en archivo CSV (W x H).
int write_u8_matrix_csv(const char *path, const uint8_t *m, int W, int H);

// Guarda una matriz uint8_t en binario plano (W x H).
int write_u8_matrix_bin(const char *path, const uint8_t *m, int W, int H);

// Calcula histograma de intensidades (0–255) sobre imagen W x H.
void histogram_u8(const uint8_t *img, int W, int H, uint64_t hist[256]);

// Guarda histograma en CSV con columnas: valor,conteo
int write_hist_csv(const char *fname, const uint64_t hist[256]);

// ---------------------------------------------------------------------
// Punto de entrada lógico del clúster
// (MPI YA debe estar inicializado desde main())
// - img_path: ruta de la imagen
// - kernel_str: máscaras Sobel en formato "x=[...] y=[...]"
// ---------------------------------------------------------------------
int cluster_run(const char *img_path, const char *kernel_str);

#endif // CLUSTER_H
