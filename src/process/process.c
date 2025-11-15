#include "process.h"
#include "external_functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>

// Función para obtener el CPU actual en Linux
#ifdef __linux__
extern int sched_getcpu(void);
#endif

void print_mascara(int *mascara_sobel) {
    printf("\n[PROCESS] Mascara Sobel recibida: \n");
    printf("\t%2d %2d %2d\n", mascara_sobel[0], mascara_sobel[1], mascara_sobel[2]);
    printf("\t%2d %2d %2d\n", mascara_sobel[3], mascara_sobel[4], mascara_sobel[5]);
    printf("\t%2d %2d %2d\n", mascara_sobel[6], mascara_sobel[7], mascara_sobel[8]);
}

// Estructura para pasar argumentos a los hilos
typedef struct {
    unsigned char *padded_image;    // Imagen con padding para facilitar el cálculo del filtro Sobel
    int height;                     // Altura de la sección de imagen sin padding   
    int width;                      // Ancho de la sección de imagen sin padding
    int *sobel_mask;                // Máscara Sobel a aplicar
    int start_row;                  // Fila inicial (inclusive) para este hilo
    int end_row;                    // Fila final (inclusive) para este hilo
    int *sobel_rst;                 // Resultado parcial del Sobel
    int thread_id;                  // ID del hilo
    int padded_width;               // Ancho de la imagen con padding
} worker_args;

// Función que ejecuta cada hilo para procesar su porción de la imagen
static void *process_image_worker(void *arg) {
    worker_args *a = (worker_args *)arg;
    int tid = a->thread_id;
    int cpu_before = -1;
#ifdef __linux__
    cpu_before = sched_getcpu();
#endif
    printf("\t[THREAD %2d] rows %4d..%4d (cpu=%2d) -> INICIO\n", tid, a->start_row, a->end_row, cpu_before);
    // Aplicar el filtro Sobel en la porción asignada
    for (int i = a->start_row; i <= a->end_row; i++) {
        for (int j = 0; j < a->width; j++) {
            int partial_sum = 0;
            for (int ki = -1; ki <= 1; ki++) {
                for (int kj = -1; kj <= 1; kj++) {
                    // Calcular la suma parcial para el pixel (i, j)
                    unsigned char image_pixel = a->padded_image[(i + ki + 1) * a->padded_width + (j + kj + 1)];
                    int mask_value = a->sobel_mask[(ki + 1) * 3 + (kj + 1)];
                    partial_sum += (int)image_pixel * mask_value;
                }
            }
            // Guardar el resultado parcial en la matriz de resultados
            a->sobel_rst[i * a->width + j] = partial_sum;
        }
    }
    int cpu_after = -1;
#ifdef __linux__
    cpu_after = sched_getcpu();
#endif
    printf("\t[THREAD %2d] rows %4d..%4d (cpu=%2d) -> FINAL\n", tid, a->start_row, a->end_row, cpu_after);
    return NULL;
}

// Función principal para procesar una sección de imagen con el filtro Sobel
metrics_node process_image(unsigned char *image_section, int height, int width, int *sobel_mask, double net_latency) {
    printf("\n[PROCESS] Iniciando procesamiento de seccion de imagen (%dx%d)...\n", width, height);
    // 1. Inicializar métricas 
    metrics_node metrics = {0.0, 0.0, 0.0, 0.0};

    // 1.1 Obtener métrica de latencia de red
    metrics.network_latency_t = net_latency;
    // 1.2 Calcular datos transferidos (imagen + máscara)
    size_t img_bytes = (size_t)height * (size_t)width * sizeof(unsigned char);
    size_t mask_bytes = 9 * sizeof(int);
    metrics.data_transferred = (double)(img_bytes + mask_bytes);

    // 1.3 Iniciar temporizador de procesamiento
    clock_t start = clock();

    // printf("[PROCESS] Imagen (porcion) recibida: \n");
    // imprimir_matriz(porcion_imagen, alto, ancho);

    // 2. Imprimir la máscara Sobel recibida
    print_mascara(sobel_mask);

    // 3. Reservar memoria para el resultado del filtro Sobel
    int *sobel_rst = (int *)calloc((size_t)height * (size_t)width, sizeof(int));
    if (sobel_rst == NULL) {
        printf("[PROCESS] ERROR: No se pudo reservar memoria para el resultado.\n");
        return metrics;
    }

    // 4. Configurar y lanzar hilos para procesar la imagen en paralelo
    int available_cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (available_cpus < 1) available_cpus = 1;

    int process_rows = (height > 0) ? height : 0;
    int num_threads = available_cpus;
    char *env = getenv("DRIVER_THREADS");
    if (env != NULL) {
        // Intentar convertir el valor de la variable de entorno a un entero
        char *endptr = NULL;
        errno = 0;
        long v = strtol(env, &endptr, 10);
        if (endptr != env && *endptr == '\0' && errno == 0 && v > 0) {
            // Valor válido
            num_threads = (int)v;
            printf("[PROCESS] DRIVER_THREADS override detected: %ld\n", v);
        } else {
            // Valor inválido
            printf("[PROCESS] WARNING: DRIVER_THREADS invalid ('%s'), usando valor por defecto %d\n", env, available_cpus);
            num_threads = available_cpus;
        }
    }

    if (process_rows > 0 && num_threads > process_rows) num_threads = process_rows;

    printf("\n[PROCESS] Núcleos disponibles: %d, hilos usados: %d\n", available_cpus, num_threads);

    if (process_rows == 0 || width <= 0) {
        // Imagen vacía, nada que procesar
        save_output_txt(sobel_rst, height, width);
        free(sobel_rst);
        printf("[PROCESS] Imagen demasiado pequeña para procesar convolución.\n");
        return metrics;
    }

    // 5. Agregar padding a la imagen para facilitar el cálculo del filtro Sobel
    int p_h = height + 2;
    int p_w = width + 2;
    unsigned char *padded = (unsigned char *)malloc((size_t)p_h * (size_t)p_w * sizeof(unsigned char));
    if (padded == NULL) {
        printf("[PROCESS] ERROR: No se pudo reservar memoria para la imagen padded.\n");
        free(sobel_rst);
        return metrics;
    }

    // Copiar la imagen original en el centro de la imagen con padding
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            padded[(i + 1) * p_w + (j + 1)] = image_section[i * width + j];
        }
    }
    
    // Rellenar las filas superior e inferior con valores duplicados
    for (int j = 0; j < width; j++) {
        padded[0 * p_w + (j + 1)] = padded[1 * p_w + (j + 1)];
        padded[(height + 1) * p_w + (j + 1)] = padded[height * p_w + (j + 1)];
    }
    
    // Rellenar las columnas izquierda y derecha con valores duplicados
    for (int i = 0; i < p_h; i++) {
        padded[i * p_w + 0] = padded[i * p_w + 1];
        padded[i * p_w + (p_w - 1)] = padded[i * p_w + (p_w - 2)];
    }

    // 6. Crear y lanzar hilos para procesar la imagen en paralelo
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
    worker_args *args = (worker_args *)malloc(sizeof(worker_args) * num_threads);
    if (threads == NULL || args == NULL) {
        // Error al reservar memoria para hilos
        printf("[PROCESS] ERROR: No se pudo reservar memoria para hilos.\n");
        free(threads);
        free(args);
        save_output_txt(sobel_rst, height, width);
        free(sobel_rst);
        return metrics;
    }

    // Dividir el trabajo entre los hilos
    int base_rows = process_rows / num_threads;
    int remainder = process_rows % num_threads;
    int current_row = 0; 

    // Crear y lanzar hilos para procesar la imagen en paralelo
    for (int t = 0; t < num_threads; t++) {
        int rows_for_thread = base_rows + (t < remainder ? 1 : 0);
        args[t].padded_image = padded;
        args[t].padded_width = p_w;
        args[t].height = height;
        args[t].width = width;
        args[t].sobel_mask = sobel_mask;
        args[t].thread_id = t;
        args[t].start_row = current_row;
        args[t].end_row = current_row + rows_for_thread - 1;
        args[t].sobel_rst = sobel_rst;

        current_row = args[t].end_row + 1;

        if (pthread_create(&threads[t], NULL, process_image_worker, &args[t]) != 0) {
            // Error al crear hilo, ejecutar en el hilo principal
            printf("[PROCESS] WARNING: fallo al crear hilo %d, ejecutando bloque en el hilo principal.\n", t);
            process_image_worker(&args[t]);
        }
    }

    // Esperar a que todos los hilos terminen
    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }

    // 7. Guardar el resultado en un archivo de salida
    save_output_txt(sobel_rst, height, width);

    // 8. Liberar memoria
    free(threads);
    free(args);
    free(padded);
    free(sobel_rst);

    // 1.4 Finalizar temporizador de procesamiento
    clock_t fin = clock();

    // 1.5 Medir el tiempo de procesamiento
    metrics.processing_t = ((double)(fin - start) / CLOCKS_PER_SEC);

    // 1.6 Calcular el throughput
    if (metrics.processing_t > 0.0) {
        metrics.throughput = metrics.data_transferred / metrics.processing_t;
    } else {
        metrics.throughput = 0.0;
    }

    // 9. Retornar las métricas obtenidas
    printf("\n[PROCESS] Tarea de procesamiento completada\n");
    return metrics;
}

// Función principal para el procesamiento de la imagen completa
metrics_node process(int *sobel_mask) {
    // Inicializar variables
    unsigned char *txt_image = NULL;
    int h = 0, w = 0;

    // Inicializar métricas
    metrics_node metrics = {0.0, 0.0, 0.0, 0.0};

    // Leer la imagen de entrada desde el archivo de texto
    if (!read_input_txt(&txt_image, &h, &w)) {
        printf("[PROCESS] ERROR: No se pudo leer la imagen de entrada.\n");
        metrics = (metrics_node){-1.0, -1.0, -1.0, -1.0};
        return metrics;
    }

    // Simular latencia de red aleatoria entre 1ms y 6ms
    double net_latency = (rand() % 5001 + 1000) / 1e6; 

    // Procesar la imagen con el filtro Sobel
    metrics = process_image(txt_image, h, w, sobel_mask, net_latency);

    print_metrics(metrics);

    // Liberar memoria de la imagen leída
    free(txt_image);

    return metrics;
}