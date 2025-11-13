#ifndef TEST_FUNCIONES_EXTERNAS_H
#define TEST_FUNCIONES_EXTERNAS_H

#include <stdio.h>

#define DIR_RESULTADOS "archivos/"
#define NOMBRE_ARCHIVO_SALIDA "resultado_sobel.txt"
#define NOMBRE_ARCHIVO_ENTRADA "matriz_test.txt"

void crear_directorio_resultados();
void guardar_resultado_txt(int* matriz_resultado, int alto, int ancho);
void imprimir_matriz(unsigned char* matriz, int alto, int ancho);
void imprimir_matriz_int(int* matriz, int alto, int ancho);

// Funcion de prueba para generar un archivo de matriz de prueba
void generar_archivo_matriz_test(int alto, int ancho);
int leer_archivo_matriz_test(unsigned char* buffer, int alto, int ancho);

#endif //TEST_FUNCIONES_EXTERNAS_H