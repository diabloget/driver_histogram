#include "procesamiento.h"
#include "funciones_externas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

metrics_node procesar_porcion(unsigned char* porcion_imagen, int alto, int ancho, int* mascara_sobel, double latencia_red) {
    // 1. Inicializar metricas
    metrics_node metricas = {0.0, 0.0, 0.0, 0.0};

    // Inicio Calculo de metricas
    // Latencia de red
    metricas.network_latency_t = latencia_red;

    // Datos transferidos (porcion de imagen recibida)
    size_t bytes_imagen = (size_t)alto * (size_t)ancho * sizeof(unsigned char);
    size_t bytes_mascara = 9 * sizeof(unsigned char);
    metricas.data_transferred = (double)(bytes_imagen + bytes_mascara);

    // 2. Iniciar el temporizador
    clock_t inicio = clock();

    printf("[TEST] Imagen (porcion) recibida: \n");
    // imprimir_matriz(porcion_imagen, alto, ancho);

    printf("[TEST] Mascara Sobel recibida: \n");
    printf("\t%2d %2d %2d\n", mascara_sobel[0], mascara_sobel[1], mascara_sobel[2]);
    printf("\t%2d %2d %2d\n", mascara_sobel[3], mascara_sobel[4], mascara_sobel[5]);
    printf("\t%2d %2d %2d\n", mascara_sobel[6], mascara_sobel[7], mascara_sobel[8]);

    // INICIO DE LA LOGIA DE CONVOLUCION
    // 1. Reservar memoria para la imagen de resultado
    int* resultado_sobel = (int*)calloc(alto * ancho, sizeof(int));
    if (resultado_sobel == NULL) {
        printf("[TEST] ERROR: No se pudo reservar memoria para el resultado.\n");
        return metricas;
    }

    // 2. Iterar sobre la imagen
    for (int i = 1; i < alto - 1; i++) {
        for (int j = 1; j < ancho - 1; j++) {
            int suma_parcial = 0;

            // 3. Aplicar el kernel/mascara de 3x3
            for (int ki = -1; ki <= 1; ki++) {
                for (int kj = -1; kj <= 1; kj++) {
                    // (i + ki) y (j + kj) son las coordenadas del pixel de la imagen original
                    unsigned char valor_pixel = porcion_imagen[(i + ki) * alto + (j + kj)];
                    // (ki + 1) y (kj + 1) son las coordenadas de la mascara
                    int valor_mascara = mascara_sobel[(ki + 1) * 3 + (kj + 1)];
                    suma_parcial += (int)valor_pixel * valor_mascara;
                }
            }
            // 4. Guardar el resultado de la matriz de salida
            resultado_sobel[i * ancho + j] = suma_parcial;
        }
    }

    printf("[TEST] Resultado (Convolucion): \n");
    // imprimir_matriz_int(resultado_sobel, alto, ancho);
    guardar_resultado_txt(resultado_sobel, alto, ancho);
    free(resultado_sobel);

    // 3. Detener el temporizador
    clock_t fin = clock();
    // 4. Calcular el tiempo de procesamiento en segundos
    metricas.processing_t = ((double)(fin - inicio) / CLOCKS_PER_SEC);

    if (metricas.processing_t > 0.0) {
        metricas.throughput = metricas.data_transferred / metricas.processing_t;
    } else {
        metricas.throughput = 0.0;
    }

    printf("[TEST] Tarea de procesamiento completada\n");
    return metricas;
}