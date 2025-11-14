#ifndef PROCESS_H
#define PROCESS_H

#include "metrics.h"

metrics_node process_image(unsigned char *image_section, int height, int width, int *sobel_mask, double net_latency);

#endif // PROCESS_H