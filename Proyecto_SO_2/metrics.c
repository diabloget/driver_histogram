#include "metrics.h"
#include <stdio.h>

void print_metrics(metrics_node metrics) {
    printf("\n[METRICS]\n");
    printf("  - Processing Time: %.6f seconds\n",  metrics.processing_t);
    printf("  - Network Latency: %.6f seconds\n",  metrics.network_latency_t);
    printf("  - Data Transferred: %.2f bytes\n",    metrics.data_transferred);
    printf("  - Throughput: %.2f bytes/second\n", metrics.throughput);
}
