#ifndef EXTERNAL_FUNCTIONS_H
#define EXTERNAL_FUNCTIONS_H

#include <stdio.h>

#define DIR_FILES "../files/"
#define OUTPUT_FILE "output.txt"
#define INPUT_FILE "input.txt"

// Función para crear el directorio de archivos si no existe
void make_files_directory();

// Funcion de prueba para generar un archivo de matriz de prueba
void make_random_matrix(int height, int width);

// Funciones para guardar y leer archivos de matriz
void save_output_txt(int *matrix, int height, int width);
int read_input_txt(unsigned char **buffer, int *height, int *width);

// Funciones para imprimir matrices en consola
void display_matrix(unsigned char *matrix, int height, int width);
void display_int_matrix(int *matrix, int height, int width);

// Funciones para cargar una imagen JPEG a formato txt y viceversa
int save_img_in_txt(const char *img_name);
int convert_txt_to_jpg(const char *img_name);

#endif // EXTERNAL_FUNCTIONS_H