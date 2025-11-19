#ifndef PROCESS_H
#define PROCESS_H

#include <stdio.h>
#include "metrics.h"

// --- Rutas de archivos ---
#define DIR_FILES   "../files/"
#define OUTPUT_FILE "output.txt"
#define INPUT_FILE  "input.txt"

// ==========================
// Procesamiento principal
// ==========================

// Función para procesar una sección de imagen con el filtro Sobel.
// Ahora recibe dos máscaras: Sobel X (Gx) y Sobel Y (Gy).
metrics_node process_image(unsigned char *image_section,
                           int height,
                           int width,
                           int *sobel_mask_gx,
                           int *sobel_mask_gy,
                           double net_latency);

// Función principal para el procesamiento de la imagen completa (modo standalone).
metrics_node process(int *sobel_mask_gx,
                     int *sobel_mask_gy);

// ==========================
// Helpers de E/S y conversión
// ==========================

void make_files_directory(void);
void make_random_matrix(int height, int width);

void save_output_txt(int *matrix, int height, int width);
void save_output_txt_as(const char *filename, int *matrix, int height, int width);
int  read_input_txt(unsigned char **buffer, int *height, int *width);

void display_matrix(unsigned char *matrix, int height, int width);
void display_int_matrix(int *matrix, int height, int width);

int save_img_in_txt(const char *img_name);
int convert_txt_to_jpg(const char *img_name);

// ==========================
// Acceso al último resultado Sobel
// ==========================

int  get_last_sobel_dims(int *height, int *width);
int *get_last_sobel_data(void);
void free_last_sobel(void);

#endif // PROCESS_H
