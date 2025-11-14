#ifndef PROCESAMIENTO_H
#define PROCESAMIENTO_H

#include "metrics.h"

metrics_node process_image(unsigned char *image_section, int height, int width, int *sobel_mask, double net_latency);

#endif // PROCESAMIENTO_H