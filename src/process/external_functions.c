#include "external_functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <linux/limits.h>
#include <jpeglib.h>

void make_files_directory() {
    struct stat st = {0};

    // Verificar si el directorio ya existe
    if (stat(DIR_FILES, &st) == -1) {
        // Crear el directorio
        if (mkdir(DIR_FILES, 0777) == -1) {
            printf("[HELPER] ERROR: No se pudo crear el directorio %s: %s\n", DIR_FILES, strerror(errno));
            return;
        }
        printf("[HELPER] Directorio %s creado exitosamente.\n", DIR_FILES);
    } else {
        printf("[HELPER] El directorio %s ya existe.\n", DIR_FILES);
    }
}

void make_input_txt(int height, int width) {
    printf("[HELPER] Generando archivo de prueba %s...\n", INPUT_FILE);
    make_files_directory();

    FILE *f = fopen(DIR_FILES INPUT_FILE, "w");
    if (f == NULL) {
        printf("[HELPER] ERROR: No se pudo crear el archivo de prueba %s: %s\n", INPUT_FILE, strerror(errno));
        return;
    }

    srand((unsigned)time(NULL));
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            unsigned char valor = (unsigned char)(rand() % 256); // Valores entre 0 y 255
            fprintf(f, "%4d ", valor);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    printf("[HELPER] Archivo de prueba %s generado exitosamente.\n", INPUT_FILE);
}

void save_output_txt(int *matrix, int height, int width) {
    make_files_directory();

    char file_name[PATH_MAX];
    sprintf(file_name, "%s%s", DIR_FILES, OUTPUT_FILE);

    FILE *f = fopen(file_name, "w");

    if (f == NULL) {
        printf("[HELPER] ERROR: No se pudo abrit el archivo %s para escribir\n", file_name);
        return;
    }

    char ruta_absoluta[PATH_MAX];
    if (realpath(file_name, ruta_absoluta) != NULL) {
        printf("[HELPER] Guardando resultado en: %s\n", ruta_absoluta);
    }

    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            fprintf(f, "%4d ", (unsigned char)matrix[i * height + j]);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    printf("[HELPER] Guardado completado.\n");
}

int read_input_txt(unsigned char *buffer, int height, int width) {
    printf("[HELPER] Leyendo matriz del archivo %s...\n", INPUT_FILE);

    char file_name[PATH_MAX];
    sprintf(file_name, "%s%s", DIR_FILES, INPUT_FILE);

    FILE *f = fopen(file_name, "r");
    if (f == NULL) {
        printf("[HELPER] ERROR: No se pudo abrir el archivo de prueba %s: %s\n", INPUT_FILE, strerror(errno));
        return 0;
    }

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int valor;
            int r = fscanf(f, "%d", &valor);
            if (r != 1) {
                if (feof(f)) {
                    printf("[HELPER] ERROR: EOF inesperado leyendo %s en posicion (%d,%d)\n", file_name, i, j);
                } else {
                    printf("[HELPER] ERROR: fallo leyendo entero de %s en posicion (%d,%d): %s\n", file_name, i, j, strerror(errno));
                }
                fclose(f);
                return 0;
            }
            buffer[i * width + j] = (unsigned char)valor;
        }
    }

    fclose(f);

    printf("[HELPER] Matriz leida del archivo %s:\n", INPUT_FILE);
    // imprimir_matriz((unsigned char*)buffer, alto, ancho);

    return 1;
}

void display_matrix(unsigned char *matriz, int height, int width) {
    for (int i = 0; i < height; i++) {
        printf("\t");
        for (int j = 0; j < width; j++) {
            printf("%3d ", matriz[i * width + j]);
        }
        printf("\n");
    }
}

void display_int_matrix(int *matriz, int height, int width) {
    for (int i = 0; i < height; i++) {
        printf("\t");
        for (int j = 0; j < width; j++) {
            printf("%4d ", matriz[i * width + j]);
        }
        printf("\n");
    }
}

int save_img_in_txt(const char *img_name, int height, int width) {
    if (!img_name || height <= 0 || width <= 0) {
        printf("[HELPER] ERROR: argumentos invalidos para guardar_imagen_gris_en_matriz_test.\n");
        return 0;
    }

    make_files_directory();

    char ruta_imagen[PATH_MAX];
    snprintf(ruta_imagen, sizeof(ruta_imagen), "%s%s", DIR_FILES, img_name);

    // Decodificar JPEG usando libjpeg
    FILE *infile = fopen(ruta_imagen, "rb");
    if (!infile) {
        printf("[HELPER] ERROR: No se pudo abrir la imagen %s: %s\n", ruta_imagen, strerror(errno));
        return 0;
    }

    // Declaraciones de libjpeg
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        printf("[HELPER] ERROR: Cabecera JPEG invalida para %s\n", ruta_imagen);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }

    jpeg_start_decompress(&cinfo);

    int src_w = (int)cinfo.output_width;
    int src_h = (int)cinfo.output_height;
    int src_comp = (int)cinfo.output_components; // 3 para RGB, 1 para gris

    size_t row_stride = (size_t)src_w * (size_t)src_comp;
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, (JDIMENSION)row_stride, 1);

    unsigned char *gray_src = (unsigned char *)malloc((size_t)src_w * (size_t)src_h);
    if (!gray_src) {
        printf("[HELPER] ERROR: Memoria insuficiente para imagen (%dx%d).\n", src_w, src_h);
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }

    // Leer filas y convertir a escala de grises (luminancia)
    int y = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        unsigned char *row = buffer[0];
        if (src_comp == 3) {
            for (int x = 0; x < src_w; x++) {
                unsigned char r = row[x * 3 + 0];
                unsigned char g = row[x * 3 + 1];
                unsigned char b = row[x * 3 + 2];
                unsigned char lum = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);
                gray_src[y * src_w + x] = lum;
            }
        } else if (src_comp == 1) {
            memcpy(&gray_src[y * src_w], row, (size_t)src_w);
        } else {
            // Otras cantidades de componentes no esperadas
            for (int x = 0; x < src_w; x++) {
                unsigned char r = row[x * src_comp + 0];
                unsigned char g = row[x * src_comp + (src_comp > 1 ? 1 : 0)];
                unsigned char b = row[x * src_comp + (src_comp > 2 ? 2 : 0)];
                unsigned char lum = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);
                gray_src[y * src_w + x] = lum;
            }
        }
        y++;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    // Remuestrear a (alto x ancho) con vecino mas cercano
    unsigned char *gray_dst = (unsigned char *)malloc((size_t)height * (size_t)width);
    if (!gray_dst) {
        printf("[HELPER] ERROR: Memoria insuficiente para matriz destino (%dx%d).\n", width, height);
        free(gray_src);
        return 0;
    }

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int src_x = (int)((long long)j * src_w / width);
            int src_y = (int)((long long)i * src_h / height);
            if (src_x < 0) src_x = 0;
            if (src_x >= src_w) src_x = src_w - 1;
            if (src_y < 0) src_y = 0;
            if (src_y >= src_h) src_y = src_h - 1;
            gray_dst[i * width + j] = gray_src[src_y * src_w + src_x];
        }
    }

    free(gray_src);

    // Guardar en matriz_test.txt con el mismo formato que generar_archivo_matriz_test
    char ruta_salida[PATH_MAX];
    snprintf(ruta_salida, sizeof(ruta_salida), "%s%s", DIR_FILES, INPUT_FILE);

    FILE *out = fopen(ruta_salida, "w");
    if (!out) {
        printf("[HELPER] ERROR: No se pudo crear %s: %s\n", ruta_salida, strerror(errno));
        free(gray_dst);
        return 0;
    }

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            fprintf(out, "%4d ", (int)gray_dst[i * width + j]);
        }
        fprintf(out, "\n");
    }

    fclose(out);
    free(gray_dst);

    char ruta_abs[PATH_MAX];
    if (realpath(ruta_salida, ruta_abs) != NULL) {
        printf("[HELPER] Imagen '%s' convertida a gris (%dx%d) y guardada en: %s\n", img_name, height, width, ruta_abs);
    } else {
        printf("[HELPER] Imagen '%s' convertida a gris (%dx%d) y guardada en: %s\n", img_name, height, width, ruta_salida);
    }

    return 1;
}

int convert_txt_to_jpg(const char *img_name) {
    make_files_directory();

    char ruta_txt[PATH_MAX];
    snprintf(ruta_txt, sizeof(ruta_txt), "%s%s", DIR_FILES, OUTPUT_FILE);

    FILE *f = fopen(ruta_txt, "r");
    if (!f) {
        printf("[HELPER] ERROR: No se pudo abrir %s: %s\n", ruta_txt, strerror(errno));
        return 0;
    }

    // Leer primera linea para contar columnas
    char linea[65536];
    int cols = 0;
    while (fgets(linea, sizeof(linea), f)) {
        // Saltar lineas vacias
        int solo_ws = 1;
        for (char *p = linea; *p; ++p) {
            if (!(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
                solo_ws = 0;
                break;
            }
        }
        if (solo_ws) continue;
        char *tok = strtok(linea, " \t\r\n");
        while (tok) {
            cols++;
            tok = strtok(NULL, " \t\r\n");
        }
        break;
    }

    if (cols <= 0) {
        printf("[HELPER] ERROR: No se detectaron columnas en %s\n", ruta_txt);
        fclose(f);
        return 0;
    }

    // Segunda pasada: volver al inicio y parsear todo, verificando columnas por fila
    rewind(f);
    size_t cap = 1024;
    size_t n = 0;
    int *vals = (int *)malloc(cap * sizeof(int));
    if (!vals) {
        fclose(f);
        printf("[HELPER] ERROR: Memoria insuficiente.\n");
        return 0;
    }

    int rows = 0;
    while (fgets(linea, sizeof(linea), f)) {
        // Contar tokens en la linea
        int line_cols = 0;
        char copia[65536];
        strncpy(copia, linea, sizeof(copia) - 1);
        copia[sizeof(copia) - 1] = '\0';
        char *tok = strtok(copia, " \t\r\n");
        while (tok) {
            line_cols++;
            tok = strtok(NULL, " \t\r\n"); 
        }
        if (line_cols == 0) continue; // linea vacia
        if (line_cols != cols) {
            printf("[HELPER] ERROR: Linea con %d columnas (esperado %d). Archivo malformado.\n", line_cols, cols);
            free(vals);
            fclose(f);
            return 0;
        }
        // Parsear y guardar
        tok = strtok(linea, " \t\r\n");
        while (tok) {
            long v = strtol(tok, NULL, 10);
            if (n >= cap) {
                cap *= 2;
                int *tmp = (int *)realloc(vals, cap * sizeof(int));
                if (!tmp) {
                    free(vals);
                    fclose(f);
                    printf("[HELPER] ERROR: Memoria insuficiente.\n");
                    return 0;
                }
                vals = tmp;
            }
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            vals[n++] = (int)v;
            tok = strtok(NULL, " \t\r\n");
        }
        rows++;
    }
    fclose(f);

    if (rows <= 0) {
        printf("[HELPER] ERROR: No se detectaron filas en %s\n", ruta_txt);
        free(vals);
        return 0;
    }

    if ((int)(n) != rows * cols) {
        printf("[HELPER] ADVERTENCIA: Conteo inconsistente n=%zu, rows=%d, cols=%d.\n", n, rows, cols);
    }

    // Convertir a buffer uint8 por fila
    unsigned char *buffer = (unsigned char *)malloc((size_t)rows * (size_t)cols);
    if (!buffer) {
        printf("[HELPER] ERROR: Memoria insuficiente para imagen %dx%d.\n", cols, rows);
        free(vals);
        return 0;
    }
    for (size_t i = 0; i < n; i++) buffer[i] = (unsigned char)vals[i];
    free(vals);

    // Preparar salida
    const char *nombre_out = (img_name && img_name[0]) ? img_name : "result.jpg";
    char ruta_jpg[PATH_MAX];
    snprintf(ruta_jpg, sizeof(ruta_jpg), "%s%s", DIR_FILES, nombre_out);

    FILE *out = fopen(ruta_jpg, "wb");
    if (!out) {
        printf("[HELPER] ERROR: No se pudo crear %s: %s\n", ruta_jpg, strerror(errno));
        free(buffer);
        return 0;
    }

    // Escribir JPEG en escala de grises
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, out);

    cinfo.image_width = (JDIMENSION)cols;
    cinfo.image_height = (JDIMENSION)rows;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 90, TRUE);

    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_pointer = &buffer[(size_t)cinfo.next_scanline * (size_t)cols];
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(out);

    char abs_out[PATH_MAX];
    if (realpath(ruta_jpg, abs_out)) {
        printf("[HELPER] Resultado Sobel exportado a imagen: %s (%dx%d)\n", abs_out, cols, rows);
    }
    else {
        printf("[HELPER] Resultado Sobel exportado a imagen: %s (%dx%d)\n", ruta_jpg, cols, rows);
    }

    free(buffer);
    return 1;
}
