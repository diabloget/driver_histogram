#include "process.h"
#include "external_functions.h"
#include "metrics.h"

#include <stdio.h>
#include <stdlib.h>

#define HEIGHT 512
#define WIDTH 512

unsigned char *test_image = NULL;

int gx_mask[9] = {
    -1, 0, 1,
    -2, 0, 2,
    -1, 0, 1};

int gy_mask[9] = {
    -1, -2, -1,
    0, 0, 0,
    1, 2, 1};

double sim_latencia()
{
    int ms = (rand() % 5001) + 1000; // Entre 1000 y 5000 microsegundos
    return (double)ms / 1e6;         // Convertir a segundos
}

int test_img()
{
    printf("\n --- TEST CON IMAGEN JPG --- \n");
    // Cargar la imagen desde archivos/, convertir a gris y guardarla como matriz_test.txt
    if (!save_img_in_txt("image.jpg", HEIGHT, WIDTH))
    {
        printf("[TEST - MASTER] ERROR: No se pudo preparar matriz_test.txt a partir de la imagen.\n");
        return 1;
    }

    // Leer la matriz generada en memoria (read header + matrix)
    int h = 0, w = 0;
    if (!read_input_txt(&test_image, &h, &w))
    {
        printf("[TEST - MASTER] ERROR: No se pudo leer matriz_test.txt\n");
        return 1;
    }

    double net_latency = sim_latencia();
    metrics_node received_metrics = process_image(test_image, h, w, gx_mask, net_latency);

    printf("[TEST - MASTER] Simulacion finalizada.\n");
    print_metrics(received_metrics);

    // Convertir el resultado sobel a una imagen JPG para inspeccion
    if (!convert_txt_to_jpg("result.jpg"))
    {
        printf("[TEST - MASTER] ADVERTENCIA: No se pudo exportar result.jpg\n");
    }

    free(test_image);

    return 0;
}

int main(void)
{
    printf("[TEST - MASTER] Iniciando simulacion de cluster...\n");

    test_img();

    return 0;
}
