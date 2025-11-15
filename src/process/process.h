#ifndef PROCESS_H
#define PROCESS_H

#include "metrics.h"

// Función para procesar una sección de imagen con el filtro Sobel
metrics_node process_image(unsigned char *image_section, int height, int width, int *sobel_mask, double net_latency);

// Función principal para el procesamiento de la imagen completa
metrics_node process(int *sobel_mask);

#endif // PROCESS_H