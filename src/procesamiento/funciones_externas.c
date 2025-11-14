#include "funciones_externas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <linux/limits.h>
#include <jpeglib.h>

void crear_directorio_resultados() {
    struct stat st = {0};

    // Verificar si el directorio ya existe
    if (stat(DIR_RESULTADOS, &st) == -1) {
        // Crear el directorio
        if (mkdir(DIR_RESULTADOS, 0777) == -1) {
            printf("[HELPER] ERROR: No se pudo crear el directorio %s: %s\n", DIR_RESULTADOS, strerror(errno));
            return;
        }
        printf("[HELPER] Directorio %s creado exitosamente.\n", DIR_RESULTADOS);
    }
    else {
        printf("[HELPER] El directorio %s ya existe.\n", DIR_RESULTADOS);
    }
}

void guardar_resultado_txt(int *matriz_resultado, int alto, int ancho) {
    crear_directorio_resultados();

    char nombre_archivo[PATH_MAX];
    sprintf(nombre_archivo, "%s%s", DIR_RESULTADOS, NOMBRE_ARCHIVO_SALIDA);

    FILE *archivo = fopen(nombre_archivo, "w");

    if (archivo == NULL) {
        printf("[HELPER] ERROR: No se pudo abrit el archivo %s para escribir\n", nombre_archivo);
        return;
    }

    char ruta_absoluta[PATH_MAX];
    if (realpath(nombre_archivo, ruta_absoluta) != NULL) {
        printf("[HELPER] Guardando resultado en: %s\n", ruta_absoluta);
    }

    for (int i = 0; i < ancho; i++) {
        for (int j = 0; j < alto; j++) {
            fprintf(archivo, "%4d ", (unsigned char)matriz_resultado[i * alto + j]);
        }
        fprintf(archivo, "\n");
    }

    fclose(archivo);
    printf("[HELPER] Guardado completado.\n");
}

void imprimir_matriz(unsigned char *matriz, int alto, int ancho) {
    for (int i = 0; i < alto; i++)  {
        printf("\t");
        for (int j = 0; j < ancho; j++) {
            printf("%3d ", matriz[i * alto + j]);
        }
        printf("\n");
    }
}

void imprimir_matriz_int(int *matriz, int alto, int ancho) {
    for (int i = 0; i < alto; i++) {
        printf("\t");
        for (int j = 0; j < ancho; j++) {
            printf("%4d ", matriz[i * alto + j]);
        }
        printf("\n");
    }
}

// Funcion de prueba para generar un archivo de matriz de prueba
void generar_archivo_matriz_test(int alto, int ancho) {
    printf("[HELPER] Generando archivo de prueba %s...\n", NOMBRE_ARCHIVO_ENTRADA);
    crear_directorio_resultados();

    FILE *f = fopen(DIR_RESULTADOS NOMBRE_ARCHIVO_ENTRADA, "w");
    if (f == NULL) {
        printf("[HELPER] ERROR: No se pudo crear el archivo de prueba %s: %s\n", NOMBRE_ARCHIVO_ENTRADA, strerror(errno));
        return;
    }

    srand((unsigned)time(NULL));
    for (int i = 0; i < alto; i++) {
        for (int j = 0; j < ancho; j++) {
            unsigned char valor = (unsigned char)(rand() % 256); // Valores entre 0 y 255
            fprintf(f, "%4d ", valor);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    printf("[HELPER] Archivo de prueba %s generado exitosamente.\n", NOMBRE_ARCHIVO_ENTRADA);
}

int leer_archivo_matriz_test(unsigned char *buffer, int alto, int ancho) {
    printf("[HELPER] Leyendo matriz del archivo %s...\n", NOMBRE_ARCHIVO_ENTRADA);

    char nombre_archivo[PATH_MAX];
    sprintf(nombre_archivo, "%s%s", DIR_RESULTADOS, NOMBRE_ARCHIVO_ENTRADA);

    FILE *f = fopen(nombre_archivo, "r");
    if (f == NULL) {
        printf("[HELPER] ERROR: No se pudo abrir el archivo de prueba %s: %s\n", NOMBRE_ARCHIVO_ENTRADA, strerror(errno));
        return 0;
    }

    for (int i = 0; i < alto; i++) {
        for (int j = 0; j < ancho; j++) {
            int valor;
            int r = fscanf(f, "%d", &valor);
            if (r != 1) {
                if (feof(f)) {
                    printf("[HELPER] ERROR: EOF inesperado leyendo %s en posicion (%d,%d)\n", nombre_archivo, i, j);
                } else {
                    printf("[HELPER] ERROR: fallo leyendo entero de %s en posicion (%d,%d): %s\n", nombre_archivo, i, j, strerror(errno));
                }
                fclose(f);
                return 0;
            }
            buffer[i * ancho + j] = (unsigned char)valor;
        }
    }

    fclose(f);

    printf("[HELPER] Matriz leida del archivo %s:\n", NOMBRE_ARCHIVO_ENTRADA);
    // imprimir_matriz((unsigned char*)buffer, alto, ancho);

    return 1;
}

int guardar_imagen_gris_en_matriz_test(const char* nombre_imagen, int alto, int ancho) {
    if (!nombre_imagen || alto <= 0 || ancho <= 0) {
        printf("[HELPER] ERROR: argumentos invalidos para guardar_imagen_gris_en_matriz_test.\n");
        return 0;
    }

    crear_directorio_resultados();

    char ruta_imagen[PATH_MAX];
    snprintf(ruta_imagen, sizeof(ruta_imagen), "%s%s", DIR_RESULTADOS, nombre_imagen);

    // Decodificar JPEG usando libjpeg
    FILE* infile = fopen(ruta_imagen, "rb");
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

    unsigned char* gray_src = (unsigned char*)malloc((size_t)src_w * (size_t)src_h);
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
        unsigned char* row = buffer[0];
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
    unsigned char* gray_dst = (unsigned char*)malloc((size_t)alto * (size_t)ancho);
    if (!gray_dst) {
        printf("[HELPER] ERROR: Memoria insuficiente para matriz destino (%dx%d).\n", ancho, alto);
        free(gray_src);
        return 0;
    }

    for (int i = 0; i < alto; i++) {
        for (int j = 0; j < ancho; j++) {
            int src_x = (int)((long long)j * src_w / ancho);
            int src_y = (int)((long long)i * src_h / alto);
            if (src_x < 0) src_x = 0; if (src_x >= src_w) src_x = src_w - 1;
            if (src_y < 0) src_y = 0; if (src_y >= src_h) src_y = src_h - 1;
            gray_dst[i * ancho + j] = gray_src[src_y * src_w + src_x];
        }
    }

    free(gray_src);

    // Guardar en matriz_test.txt con el mismo formato que generar_archivo_matriz_test
    char ruta_salida[PATH_MAX];
    snprintf(ruta_salida, sizeof(ruta_salida), "%s%s", DIR_RESULTADOS, NOMBRE_ARCHIVO_ENTRADA);

    FILE* out = fopen(ruta_salida, "w");
    if (!out) {
        printf("[HELPER] ERROR: No se pudo crear %s: %s\n", ruta_salida, strerror(errno));
        free(gray_dst);
        return 0;
    }

    for (int i = 0; i < alto; i++) {
        for (int j = 0; j < ancho; j++) {
            fprintf(out, "%4d ", (int)gray_dst[i * ancho + j]);
        }
        fprintf(out, "\n");
    }

    fclose(out);
    free(gray_dst);

    char ruta_abs[PATH_MAX];
    if (realpath(ruta_salida, ruta_abs) != NULL) {
        printf("[HELPER] Imagen '%s' convertida a gris (%dx%d) y guardada en: %s\n", nombre_imagen, alto, ancho, ruta_abs);
    } else {
        printf("[HELPER] Imagen '%s' convertida a gris (%dx%d) y guardada en: %s\n", nombre_imagen, alto, ancho, ruta_salida);
    }

    return 1;
}

int convertir_resultado_txt_a_jpg(const char* nombre_salida_jpg) {
    crear_directorio_resultados();

    char ruta_txt[PATH_MAX];
    snprintf(ruta_txt, sizeof(ruta_txt), "%s%s", DIR_RESULTADOS, NOMBRE_ARCHIVO_SALIDA);

    FILE* f = fopen(ruta_txt, "r");
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
        for (char* p = linea; *p; ++p) { if (!(*p==' '||*p=='\t'||*p=='\n' || *p=='\r')) { solo_ws = 0; break; } }
        if (solo_ws) continue;
        char* tok = strtok(linea, " \t\r\n");
        while (tok) { cols++; tok = strtok(NULL, " \t\r\n"); }
        break;
    }

    if (cols <= 0) {
        printf("[HELPER] ERROR: No se detectaron columnas en %s\n", ruta_txt);
        fclose(f);
        return 0;
    }

    // Segunda pasada: volver al inicio y parsear todo, verificando columnas por fila
    rewind(f);
    size_t cap = 1024; size_t n = 0;
    int* vals = (int*)malloc(cap * sizeof(int));
    if (!vals) { fclose(f); printf("[HELPER] ERROR: Memoria insuficiente.\n"); return 0; }

    int rows = 0;
    while (fgets(linea, sizeof(linea), f)) {
        // Contar tokens en la linea
        int line_cols = 0;
        char copia[65536];
        strncpy(copia, linea, sizeof(copia)-1); copia[sizeof(copia)-1] = '\0';
        char* tok = strtok(copia, " \t\r\n");
        while (tok) { line_cols++; tok = strtok(NULL, " \t\r\n"); }
        if (line_cols == 0) continue; // linea vacia
        if (line_cols != cols) {
            printf("[HELPER] ERROR: Linea con %d columnas (esperado %d). Archivo malformado.\n", line_cols, cols);
            free(vals); fclose(f); return 0;
        }
        // Parsear y guardar
        tok = strtok(linea, " \t\r\n");
        while (tok) {
            long v = strtol(tok, NULL, 10);
            if (n >= cap) {
                cap *= 2; int* tmp = (int*)realloc(vals, cap * sizeof(int));
                if (!tmp) { free(vals); fclose(f); printf("[HELPER] ERROR: Memoria insuficiente.\n"); return 0; }
                vals = tmp;
            }
            if (v < 0) v = 0; if (v > 255) v = 255;
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
    unsigned char* buffer = (unsigned char*)malloc((size_t)rows * (size_t)cols);
    if (!buffer) {
        printf("[HELPER] ERROR: Memoria insuficiente para imagen %dx%d.\n", cols, rows);
        free(vals);
        return 0;
    }
    for (size_t i = 0; i < n; i++) buffer[i] = (unsigned char)vals[i];
    free(vals);

    // Preparar salida
    const char* nombre_out = (nombre_salida_jpg && nombre_salida_jpg[0]) ? nombre_salida_jpg : "result.jpg";
    char ruta_jpg[PATH_MAX];
    snprintf(ruta_jpg, sizeof(ruta_jpg), "%s%s", DIR_RESULTADOS, nombre_out);

    FILE* out = fopen(ruta_jpg, "wb");
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
    } else {
        printf("[HELPER] Resultado Sobel exportado a imagen: %s (%dx%d)\n", ruta_jpg, cols, rows);
    }

    free(buffer);
    return 1;
}
