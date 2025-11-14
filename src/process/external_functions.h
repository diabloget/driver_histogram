#ifndef EXTERNAL_FUNCTIONS_H
#define EXTERNAL_FUNCTIONS_H

#include <stdio.h>

#define DIR_FILES "../files/" // Quitar ../ cuando se use en produccion
#define OUTPUT_FILE "output.txt"
#define INPUT_FILE "input.txt"

void make_files_directory();

// Funcion de prueba para generar un archivo de matriz de prueba
void make_input_txt(int height, int width);

// Funciones para guardar y leer archivos de matriz
void save_output_txt(int *matrix, int height, int width);
int read_input_txt(unsigned char **buffer, int *height, int *width);

// Funciones para imprimir matrices en consola
void display_matrix(unsigned char *matrix, int height, int width);
void display_int_matrix(int *matrix, int height, int width);

// Funciones de prueba para cargar una imagen JPEG, y aplicar filtro sobel
int save_img_in_txt(const char *img_name, int height, int width);
int convert_txt_to_jpg(const char *img_name);

#endif // EXTERNAL_FUNCTIONS_H