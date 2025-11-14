#include "procesamiento.h"
#include "funciones_externas.h"
#include "metrics.h"

#include <stdio.h>
#include <stdlib.h>


#define ALTO_TEST 512
#define ANCHO_TEST 512

unsigned char imagen_test[ALTO_TEST][ANCHO_TEST];

int mascara_gx[9] = {
    -1, 0, 1,
    -2, 0, 2,
    -1, 0, 1
};

int mascara_gy[9] = {
    -1, -2, -1,
     0,  0,  0,
     1,  2,  1
};

double sim_latencia() {
    int microsegundos = (rand() % 5001) + 1000; // Entre 1000 y 5000 microsegundos
    return (double)microsegundos / 1e6; // Convertir a segundos
}

int test_img() {
    printf("\n --- TEST CON IMAGEN JPG --- \n");
    // Cargar la imagen desde archivos/, convertir a gris y guardarla como matriz_test.txt
    if (!guardar_imagen_gris_en_matriz_test("image.jpg", ALTO_TEST, ANCHO_TEST)) {
        printf("[TEST - MASTER] ERROR: No se pudo preparar matriz_test.txt a partir de la imagen.\n");
        return 1;
    }

    // Leer la matriz generada en memoria
    if (!leer_archivo_matriz_test((unsigned char*)imagen_test, ALTO_TEST, ANCHO_TEST)) {
        printf("[TEST - MASTER] ERROR: No se pudo leer matriz_test.txt\n");
        return 1;
    }

    double latencia_red = sim_latencia();
    metrics_node metricas_recibidas = procesar_porcion((unsigned char*)imagen_test, ALTO_TEST, ANCHO_TEST, mascara_gy, latencia_red);

    printf("[TEST - MASTER] Simulacion finalizada.\n");
    print_metrics(metricas_recibidas);

    // Convertir el resultado sobel a una imagen JPG para inspeccion
    if (!convertir_resultado_txt_a_jpg("result.jpg")) {
        printf("[TEST - MASTER] ADVERTENCIA: No se pudo exportar result.jpg\n");
    }

    return 0;
}

int test_matriz() {
    printf("\n --- TEST CON MATRIZ DE PRUEBA --- \n");
    // Cargar la matriz de prueba desde matriz_test.txt
    generar_archivo_matriz_test(ALTO_TEST, ANCHO_TEST);

    leer_archivo_matriz_test((unsigned char*)imagen_test, ALTO_TEST, ANCHO_TEST);

    double latencia_red = sim_latencia();
    metrics_node metricas_recibidas = procesar_porcion((unsigned char*)imagen_test, ALTO_TEST, ANCHO_TEST, mascara_gy, latencia_red);

    printf("[TEST - MASTER] Simulacion finalizada.\n");
    print_metrics(metricas_recibidas);

    return 0;
}

int main(void) {
    printf("[TEST - MASTER] Iniciando simulacion de cluster...\n");

    test_img();

    // test_matriz();

    return 0;
}
