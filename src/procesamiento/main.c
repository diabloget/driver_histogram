#include "procesamiento.h"
#include "funciones_externas.h"
#include "metrics.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#define ALTO_TEST 512
#define ANCHO_TEST 512

unsigned char imagen_test[ALTO_TEST][ANCHO_TEST];

int mascara_gx[9] = {
    -1, 0, 1,
    -2, 0, 2,
    -1, 0, 1
};

double sim_latencia() {
    int microsegundos = (rand() % 5001) + 1000; // Entre 1000 y 5000 microsegundos
    return (double)microsegundos / 1e6; // Convertir a segundos
}

int main(void) {
    printf("[TEST - MASTER] Iniciando simulacion de cluster...\n");

    generar_archivo_matriz_test(ALTO_TEST, ANCHO_TEST);
    leer_archivo_matriz_test((unsigned char*)imagen_test, ALTO_TEST, ANCHO_TEST);

    double latencia_red = sim_latencia();
    metrics_node metricas_recibidas = procesar_porcion((unsigned char*)imagen_test, ALTO_TEST, ANCHO_TEST, mascara_gx, latencia_red);

    printf("[TEST - MASTER] Simulacion finalizada.\n");
    print_metrics(metricas_recibidas);

    return 0;
}
