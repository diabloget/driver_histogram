#include "procesamiento.h"
#include "funciones_externas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

void print_mascara(int *mascara_sobel)
{
    printf("[TEST] Mascara Sobel recibida: \n");
    printf("\t%2d %2d %2d\n", mascara_sobel[0], mascara_sobel[1], mascara_sobel[2]);
    printf("\t%2d %2d %2d\n", mascara_sobel[3], mascara_sobel[4], mascara_sobel[5]);
    printf("\t%2d %2d %2d\n", mascara_sobel[6], mascara_sobel[7], mascara_sobel[8]);
}

metrics_node process_image(unsigned char *image_section, int height, int width, int *sobel_mask, double net_latency)
{
    // 1. Inicializar metricas
    metrics_node metrics = {0.0, 0.0, 0.0, 0.0};

    // Inicio Calculo de metricas
    // Latencia de red
    metrics.network_latency_t = net_latency;
    // Datos transferidos (porcion de imagen recibida)
    size_t img_bytes = (size_t)height * (size_t)width * sizeof(unsigned char);
    size_t mask_bytes = 9 * sizeof(unsigned char);
    metrics.data_transferred = (double)(img_bytes + mask_bytes);

    // 2. Iniciar el temporizador
    clock_t start = clock();

    // printf("[TEST] Imagen (porcion) recibida: \n");
    // imprimir_matriz(porcion_imagen, alto, ancho);

    print_mascara(sobel_mask);

    // INICIO DE LA LOGIA DE CONVOLUCION
    // 1. Reservar memoria para la imagen de resultado
    int *sobel_rst = (int *)calloc(height * width, sizeof(int));
    if (sobel_rst == NULL)
    {
        printf("[TEST] ERROR: No se pudo reservar memoria para el resultado.\n");
        return metrics;
    }

    // 2. Iterar sobre la imagen
    for (int i = 1; i < height - 1; i++)
    {
        for (int j = 1; j < width - 1; j++)
        {
            int partial_sum = 0;

            // 3. Aplicar el mascara de 3x3
            for (int ki = -1; ki <= 1; ki++)
            {
                for (int kj = -1; kj <= 1; kj++)
                {
                    // (i + ki) y (j + kj) son las coordenadas del pixel de la imagen original
                    unsigned char image_pixel = image_section[(i + ki) * width + (j + kj)];
                    // (ki + 1) y (kj + 1) son las coordenadas de la mascara
                    int mask_value = sobel_mask[(ki + 1) * 3 + (kj + 1)];
                    partial_sum += (int)image_pixel * mask_value;
                }
            }
            // 4. Guardar el resultado de la matriz de salida
            sobel_rst[i * width + j] = partial_sum;
        }
    }

    // printf("[TEST] Resultado (Convolucion): \n");
    // imprimir_matriz_int(resultado_sobel, alto, ancho);
    save_output_txt(sobel_rst, height, width);
    free(sobel_rst);

    // 3. Detener el temporizador
    clock_t fin = clock();
    // 4. Calcular el tiempo de procesamiento en segundos
    metrics.processing_t = ((double)(fin - start) / CLOCKS_PER_SEC);

    if (metrics.processing_t > 0.0)
    {
        metrics.throughput = metrics.data_transferred / metrics.processing_t;
    }
    else
    {
        metrics.throughput = 0.0;
    }

    printf("[TEST] Tarea de procesamiento completada\n");
    return metrics;
}