#include "external_functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <linux/limits.h>
#include <jpeglib.h>

// Función para crear el directorio de archivos si no existe
void make_files_directory()
{
    struct stat st = {0};
    // Verificar si el directorio ya existe
    if (stat(DIR_FILES, &st) == -1)
    {
        // Crear el directorio
        if (mkdir(DIR_FILES, 0777) == -1)
        {
            // Error al crear el directorio
            printf("[HELPER] ERROR: No se pudo crear el directorio %s: %s\n", DIR_FILES, strerror(errno));
            return;
        }
        // printf("[HELPER] Directorio %s creado exitosamente.\n", DIR_FILES);
    }
    else
    {
        // printf("[HELPER] El directorio %s ya existe.\n", DIR_FILES);
    }
}

// Función para generar un archivo de texto con una matriz de entrada aleatoria
void make_random_matrix(int height, int width) {
    // printf("[HELPER] Generando archivo de prueba %s...\n", INPUT_FILE);
    make_files_directory();

    // Crear la ruta completa del archivo
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s%s", DIR_FILES, INPUT_FILE);

    FILE *f = fopen(path, "w");
    if (f == NULL) {
        // Error al abrir el archivo
        printf("[HELPER] ERROR: No se pudo abrir el archivo de prueba %s: %s\n", INPUT_FILE, strerror(errno));
        return;
    }

    // Generar valores aleatorios para la matriz
    srand((unsigned)time(NULL));
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            unsigned char valor = (unsigned char)(rand() % 256); // Valores entre 0 y 255
            fprintf(f, "%4d ", valor);
        }
        fprintf(f, "\n");
    }

    // printf("[HELPER] Archivo de prueba %s generado exitosamente.\n", INPUT_FILE);
    fclose(f);
}

// Función para guardar una matriz de enteros en un archivo de texto
void save_output_txt(int *matrix, int height, int width) {
    make_files_directory();

    // Crear la ruta completa del archivo
    char file_name[PATH_MAX];
    sprintf(file_name, "%s%s", DIR_FILES, OUTPUT_FILE);

    FILE *f = fopen(file_name, "w");
    if (f == NULL) {
        // Error al abrir el archivo
        printf("[HELPER] ERROR: No se pudo abrit el archivo %s para escribir\n", file_name);
        return;
    }

    char ruta_absoluta[PATH_MAX];
    if (realpath(file_name, ruta_absoluta) != NULL) {
        // printf("[HELPER] Guardando resultado en: %s\n", ruta_absoluta);
    }

    // Primera línea - dimensiones de la matriz
    fprintf(f, "%d %d\n", height, width);

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // Escribir el valor de la matriz en el archivo
            fprintf(f, "%d ", matrix[i * width + j]);
        }
        fprintf(f, "\n");
    }

    // printf("[HELPER] Guardado completado.\n");
    fclose(f);
}

// Función para leer una matriz de un archivo de texto
int read_input_txt(unsigned char **buffer, int *height, int *width) {
    // Crear la ruta completa del archivo
    char file_name[PATH_MAX];
    snprintf(file_name, sizeof(file_name), "%s%s", DIR_FILES, INPUT_FILE);

    FILE *f = fopen(file_name, "r");
    if (f == NULL) {
        // Error al abrir el archivo
        printf("[HELPER] ERROR: No se pudo abrir el archivo de prueba %s: %s\n", file_name, strerror(errno));
        return 0;
    }

    // Leer dimensiones de la matriz
    int h = 0, w = 0;
    if (fscanf(f, "%d %d", &h, &w) != 2) {
        // Error al leer dimensiones
        printf("[HELPER] ERROR: Formato invalido en cabecera de %s (se espera: height width)\n", file_name);
        fclose(f);
        return 0;
    }

    // Validar dimensiones
    if (h <= 0 || w <= 0) {
        // Dimensiones invalidas
        printf("[HELPER] ERROR: dimensiones invalidas en cabecera: %d x %d\n", h, w);
        fclose(f);
        return 0;
    }

    // Reservar memoria para la matriz
    size_t needed = (size_t)h * (size_t)w;
    unsigned char *buf = (unsigned char *)malloc(needed);
    if (!buf) {
        // Error de memoria
        printf("[HELPER] ERROR: Memoria insuficiente para leer matriz %dx%d\n", h, w);
        fclose(f);
        return 0;
    }

    // Leer los valores de la matriz
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            // Leer valor
            int valor;
            int r = fscanf(f, "%d", &valor);
            if (r != 1) {
                // Error al leer valor
                if (feof(f)) {
                    printf("[HELPER] ERROR: EOF inesperado leyendo %s en posicion (%d,%d)\n", file_name, i, j);
                } else {
                    printf("[HELPER] ERROR: fallo leyendo entero de %s en posicion (%d,%d): %s\n", file_name, i, j, strerror(errno));
                }
                // liberar memoria y cerrar archivo
                free(buf);
                fclose(f);
                return 0;
            }
            // Asegurar rango 0-255
            if (valor < 0) valor = 0;
            if (valor > 255) valor = 255;   
            // Asignar valor a la matriz
            buf[i * w + j] = (unsigned char)valor;
        }
    }

    fclose(f);

    // Asignar resultados
    *buffer = buf;
    *height = h;
    *width = w;
    return 1;
}

// Función para mostrar una matriz de unsigned char en consola
void display_matrix(unsigned char *matriz, int height, int width) {
    for (int i = 0; i < height; i++) {
        printf("\t");
        for (int j = 0; j < width; j++) {
            printf("%3d ", matriz[i * width + j]);
        }
        printf("\n");
    }
}

// Función para mostrar una matriz de enteros en consola
void display_int_matrix(int *matriz, int height, int width) {
    for (int i = 0; i < height; i++) {
        printf("\t");
        for (int j = 0; j < width; j++) {
            printf("%4d ", matriz[i * width + j]);
        }
        printf("\n");
    }
}

// Función para guardar una imagen JPEG en escala de grises en un archivo de texto
int save_img_in_txt(const char *img_name) {
    // Verificar argumentos
    if (!img_name) {
        printf("[HELPER] ERROR: argumento invalido para guardar imagen gris en matriz_test.\n");
        return 0;
    }

    make_files_directory();

    // Crear la ruta completa de la imagen
    char ruta_imagen[PATH_MAX];
    snprintf(ruta_imagen, sizeof(ruta_imagen), "%s%s", DIR_FILES, img_name);

    FILE *infile = fopen(ruta_imagen, "rb");
    if (!infile) {
        // Error al abrir archivo
        printf("[HELPER] ERROR: No se pudo abrir la imagen %s: %s\n", ruta_imagen, strerror(errno));
        return 0;
    }

    // Declaraciones de libjpeg
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    // Inicializar descompresor JPEG
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);

    // Leer cabecera JPEG
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        // Error en cabecera JPEG
        printf("[HELPER] ERROR: Cabecera JPEG invalida para %s\n", ruta_imagen);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }

    // Iniciar descompresión JPEG
    jpeg_start_decompress(&cinfo);

    // Obtener dimensiones y componentes de la imagen
    int src_w = (int)cinfo.output_width;
    int src_h = (int)cinfo.output_height;
    int src_comp = (int)cinfo.output_components;

    // Calcular el tamaño de una fila en bytes
    size_t row_stride = (size_t)src_w * (size_t)src_comp;
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, (JDIMENSION)row_stride, 1);

    // Reservar memoria para la imagen en escala de grises
    unsigned char *gray_src = (unsigned char *)malloc((size_t)src_w * (size_t)src_h);
    if (!gray_src) {
        // Error de memoria
        printf("[HELPER] ERROR: Memoria insuficiente para imagen (%dx%d).\n", src_w, src_h);
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }

    // Leer filas y convertir a escala de grises (luminancia)
    int y = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        // Leer una fila
        jpeg_read_scanlines(&cinfo, buffer, 1);
        unsigned char *row = buffer[0];
        if (src_comp == 3) {
            // Convertir RGB a luminancia
            for (int x = 0; x < src_w; x++) {
                unsigned char r = row[x * 3 + 0];
                unsigned char g = row[x * 3 + 1];
                unsigned char b = row[x * 3 + 2];
                // Calcular luminancia usando la fórmula estándar
                unsigned char lum = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);
                gray_src[y * src_w + x] = lum;
            }
        } else if (src_comp == 1) {
            // Copiar directamente la fila en escala de grises
            memcpy(&gray_src[y * src_w], row, (size_t)src_w);
        } else {
            for (int x = 0; x < src_w; x++) {
                // Convertir a luminancia para otros formatos
                unsigned char r = row[x * src_comp + 0];
                unsigned char g = row[x * src_comp + (src_comp > 1 ? 1 : 0)];
                unsigned char b = row[x * src_comp + (src_comp > 2 ? 2 : 0)];
                unsigned char lum = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);
                gray_src[y * src_w + x] = lum;
            }
        }
        y++;
    }

    // Finalizar descompresión JPEG
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    // Crear la ruta completa del archivo de salida
    char ruta_salida[PATH_MAX];
    snprintf(ruta_salida, sizeof(ruta_salida), "%s%s", DIR_FILES, INPUT_FILE);

    // Guardar la imagen en escala de grises en el archivo de texto
    FILE *out = fopen(ruta_salida, "w");
    if (!out) {
        // Error al crear archivo de salida
        printf("[HELPER] ERROR: No se pudo crear %s: %s\n", ruta_salida, strerror(errno));
        free(gray_src);
        return 0;
    }

    // Escribir dimensiones en la primera línea
    fprintf(out, "%d %d\n", src_h, src_w);

    for (int i = 0; i < src_h; i++) {
        for (int j = 0; j < src_w; j++) {
            // Escribir valor de píxel en el archivo
            fprintf(out, "%d ", (int)gray_src[i * src_w + j]);
        }
        fprintf(out, "\n");
    }

    // Cerrar el archivo de salida
    fclose(out);
    free(gray_src);

    return 1;
}

// Función para convertir un archivo de texto con matriz a una imagen JPEG en escala de grises
int convert_txt_to_jpg(const char *img_name) {
    make_files_directory();

    // Crear la ruta completa del archivo de texto
    char ruta_txt[PATH_MAX];
    snprintf(ruta_txt, sizeof(ruta_txt), "%s%s", DIR_FILES, OUTPUT_FILE);

    // Abrir el archivo de texto para lectura
    FILE *f = fopen(ruta_txt, "r");
    if (!f) {
        // Error al abrir archivo de texto
        printf("[HELPER] ERROR: No se pudo abrir %s: %s\n", ruta_txt, strerror(errno));
        return 0;
    }

    // Leer dimensiones de la imagen
    int rows = 0, cols = 0;
    if (fscanf(f, "%d %d", &rows, &cols) != 2) {
        // Error en formato de cabecera
        printf("[HELPER] ERROR: formato invalido en cabecera de %s (se espera: height width)\n", ruta_txt);
        fclose(f);
        return 0;
    }

    // Validar dimensiones
    if (rows <= 0 || cols <= 0) {
        // Error en dimensiones
        printf("[HELPER] ERROR: dimensiones invalidas en cabecera: %d x %d\n", rows, cols);
        fclose(f);
        return 0;
    }

    // Reservar memoria para los valores de píxeles
    int *vals = (int *)malloc((size_t)rows * (size_t)cols * sizeof(int));
    if (!vals) {
        // Error de memoria
        printf("[HELPER] ERROR: Memoria insuficiente.\n");
        fclose(f);
        return 0;
    }

    // Leer valores de píxeles
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            long v;
            if (fscanf(f, "%ld", &v) != 1) {
                // Error leyendo valor
                printf("[HELPER] ERROR: fallo leyendo valor en posicion (%d,%d)\n", i, j);
                free(vals);
                fclose(f);
                return 0;
            }
            // Asegurar rango 0-255
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            // Asignar valor
            vals[i * cols + j] = (int)v;
        }
    }

    // Cerrar el archivo de texto
    fclose(f);

    // Reservar memoria para el buffer de la imagen JPEG
    unsigned char *buffer = (unsigned char *)malloc((size_t)rows * (size_t)cols);
    if (!buffer) {
        // Error de memoria
        printf("[HELPER] ERROR: Memoria insuficiente para imagen %dx%d.\n", cols, rows);
        free(vals);
        return 0;
    }
    // Copiar valores a buffer de imagen
    for (int i = 0; i < rows * cols; i++) buffer[i] = (unsigned char)vals[i];
    
    // Liberar memoria de valores originales
    free(vals);

    // Crear la ruta completa del archivo JPEG de salida
    const char *nombre_out = (img_name && img_name[0]) ? img_name : "result.jpg";
    char ruta_jpg[PATH_MAX];
    snprintf(ruta_jpg, sizeof(ruta_jpg), "%s%s", DIR_FILES, nombre_out);

    // Abrir archivo JPEG para escritura
    FILE *out = fopen(ruta_jpg, "wb");
    if (!out) {
        printf("[HELPER] ERROR: No se pudo crear %s: %s\n", ruta_jpg, strerror(errno));
        free(buffer);
        return 0;
    }

    // Configurar compresor JPEG
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, out);

    // Configurar parámetros de la imagen JPEG
    cinfo.image_width = (JDIMENSION)cols;
    cinfo.image_height = (JDIMENSION)rows;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;

    // Establecer valores predeterminados y calidad
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 90, TRUE);

    // Iniciar compresión JPEG
    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
        // Escribir una fila
        JSAMPROW row_pointer = &buffer[(size_t)cinfo.next_scanline * (size_t)cols];
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    // Finalizar compresión JPEG
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    // Cerrar archivo JPEG
    fclose(out);

    // Liberar memoria del buffer de imagen
    free(buffer);
    return 1;
}
