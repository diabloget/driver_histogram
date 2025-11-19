#ifndef METRICS_H
#define METRICS_H

typedef struct {
    double processing_t;        // Tiempo de procesamiento
    double network_latency_t;   // Latencia de red
    double data_transferred;    // Datos transferidos en bytes
    double throughput;          // Rendimiento en bytes por segundo
} metrics_node;

void print_metrics(metrics_node metrics);

#endif // METRICS_H
