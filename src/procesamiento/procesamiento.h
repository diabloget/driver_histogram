#ifndef PROCESAMIENTO_H
#define PROCESAMIENTO_H

#include "metrics.h"

metrics_node procesar_porcion(unsigned char* porcion_imagen, int alto, int ancho, int* mascara_sobel, double latencia_red);

#endif //PROCESAMIENTO_H