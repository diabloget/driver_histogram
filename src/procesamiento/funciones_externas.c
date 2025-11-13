#include "funciones_externas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <linux/limits.h>

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
            fprintf(archivo, "%4d ", matriz_resultado[i * alto + j]);
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
            fprintf(f, "%d ", valor);
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