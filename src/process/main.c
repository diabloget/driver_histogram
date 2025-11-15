#include "process.h"
#include "external_functions.h"
#include "metrics.h"

#include <stdio.h>
#include <stdlib.h>

int gx_mask[9] = {
    -1, 0, 1,
    -2, 0, 2,
    -1, 0, 1};

int gy_mask[9] = {
    -1, -2, -1,
    0, 0, 0,
    1, 2, 1};

int test_process() {
    printf("\n --- TEST DE FUNCION PROCESS --- \n");

    // Preparar la matriz_test.txt a partir de una imagen JPG
    if (!save_img_in_txt("image4.jpg")) {
        printf("[TEST - MASTER] ERROR: No se pudo preparar matriz_test.txt a partir de la imagen.\n");
        return 1;
    }

    // Procesar la imagen con el filtro Sobel
    metrics_node received_metrics = process(gy_mask);

    printf("[TEST - MASTER] Simulacion finalizada.\n");

    // Convertir el resultado sobel a una imagen JPG para inspeccion
    if (!convert_txt_to_jpg("result.jpg")) {
        printf("[TEST - MASTER] ADVERTENCIA: No se pudo exportar result.jpg\n");
    }

    return 0;
}

int main(void) {
    printf("[TEST - MASTER] Iniciando simulacion de cluster...\n");

    test_process();

    return 0;
}
