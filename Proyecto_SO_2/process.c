#include "process.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <linux/limits.h>
#include <jpeglib.h>
#include <math.h>

// Función para obtener el CPU actual en Linux
#ifdef __linux__
extern int sched_getcpu(void);
#endif

// ----------------------------------------------------------------------
// Buffer global para el último resultado Sobel
// ----------------------------------------------------------------------
static int *g_last_sobel = NULL;
static int  g_last_h     = 0;
static int  g_last_w     = 0;

// ----------------------------------------------------------------------
// Impresión de máscara
// ----------------------------------------------------------------------
static void print_mascara(const char *label, int *mascara_sobel) {
    printf("\n[PROCESS] Mascara %s: \n", label);
    printf("\t%4d %4d %4d\n", mascara_sobel[0], mascara_sobel[1], mascara_sobel[2]);
    printf("\t%4d %4d %4d\n", mascara_sobel[3], mascara_sobel[4], mascara_sobel[5]);
    printf("\t%4d %4d %4d\n", mascara_sobel[6], mascara_sobel[7], mascara_sobel[8]);
}

// ----------------------------------------------------------------------
// Estructura para pasar argumentos a los hilos
// ----------------------------------------------------------------------
typedef struct {
    unsigned char *padded_image; // Imagen con padding
    int height;                  // Altura sin padding
    int width;                   // Ancho sin padding
    int *sobel_mask_gx;          // Máscara Sobel en X
    int *sobel_mask_gy;          // Máscara Sobel en Y
    int start_row;               // Fila inicial (inclusive)
    int end_row;                 // Fila final (inclusive)
    int *sobel_rst;              // Resultado parcial del Sobel
    int thread_id;               // ID del hilo
    int padded_width;            // Ancho de la imagen con padding
} worker_args;

// ----------------------------------------------------------------------
// Worker de cada hilo
// ----------------------------------------------------------------------
static void *process_image_worker(void *arg) {
    worker_args *a = (worker_args *)arg;
    int tid = a->thread_id;
    int cpu_before = -1;
#ifdef __linux__
    cpu_before = sched_getcpu();
#endif
    printf("\t[THREAD %2d] rows %4d..%4d (cpu=%2d) -> INICIO\n",
           tid, a->start_row, a->end_row, cpu_before);

    // Aplicar el filtro Sobel (Gx, Gy) y magnitud |Gx| + |Gy|
    for (int i = a->start_row; i <= a->end_row; i++) {
        for (int j = 0; j < a->width; j++) {
            int gx = 0;
            int gy = 0;
            for (int ki = -1; ki <= 1; ki++) {
                for (int kj = -1; kj <= 1; kj++) {
                    unsigned char image_pixel =
                        a->padded_image[(i + ki + 1) * a->padded_width + (j + kj + 1)];
                    int idx = (ki + 1) * 3 + (kj + 1);
                    int mvx = a->sobel_mask_gx[idx];
                    int mvy = a->sobel_mask_gy[idx];
                    gx += (int)image_pixel * mvx;
                    gy += (int)image_pixel * mvy;
                }
            }
            int mag = abs(gx) + abs(gy);   // aproximación clásica sin sqrt
            if (mag < 0)   mag = 0;
            if (mag > 255) mag = 255;
            a->sobel_rst[i * a->width + j] = mag;
        }
    }
    int cpu_after = -1;
#ifdef __linux__
    cpu_after = sched_getcpu();
#endif
    printf("\t[THREAD %2d] rows %4d..%4d (cpu=%2d) -> FINAL\n",
           tid, a->start_row, a->end_row, cpu_after);
    return NULL;
}

// ----------------------------------------------------------------------
// Procesamiento de una sección de imagen
// ----------------------------------------------------------------------
metrics_node process_image(unsigned char *image_section,
                           int height,
                           int width,
                           int *sobel_mask_gx,
                           int *sobel_mask_gy,
                           double net_latency) {
    printf("\n[PROCESS] Iniciando procesamiento de seccion de imagen (%dx%d)...\n",
           width, height);

    // 1. Inicializar métricas 
    metrics_node metrics = {0.0, 0.0, 0.0, 0.0};

    // 1.1 Obtener métrica de latencia de red
    metrics.network_latency_t = net_latency;
    // 1.2 Calcular datos transferidos (imagen + dos máscaras)
    size_t img_bytes  = (size_t)height * (size_t)width * sizeof(unsigned char);
    size_t mask_bytes = 9 * sizeof(int) * 2;
    metrics.data_transferred = (double)(img_bytes + mask_bytes);

    // 1.3 Iniciar temporizador de procesamiento
    clock_t start = clock();

    // 2. Imprimir las máscaras Sobel recibidas
    print_mascara("Sobel X", sobel_mask_gx);
    print_mascara("Sobel Y", sobel_mask_gy);

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
    int num_threads  = available_cpus;

    char *env = getenv("DRIVER_THREADS");
    if (env != NULL) {
        // Intentar convertir el valor de la variable de entorno a un entero
        char *endptr = NULL;
        errno = 0;
        long v = strtol(env, &endptr, 10);
        if (endptr != env && *endptr == '\0' && errno == 0 && v > 0) {
            num_threads = (int)v;
            printf("[PROCESS] DRIVER_THREADS override detected: %ld\n", v);
        } else {
            printf("[PROCESS] WARNING: DRIVER_THREADS invalid ('%s'), usando valor por defecto %d\n",
                   env, available_cpus);
            num_threads = available_cpus;
        }
    }

    if (process_rows > 0 && num_threads > process_rows)
        num_threads = process_rows;

    printf("\n[PROCESS] Núcleos disponibles: %d, hilos usados: %d\n",
           available_cpus, num_threads);

    if (process_rows == 0 || width <= 0) {
        printf("[PROCESS] Imagen demasiado pequeña para procesar convolución.\n");
        free(sobel_rst);
        return metrics;
    }

    // 5. Agregar padding a la imagen para facilitar el cálculo del filtro Sobel
    int p_h = height + 2;
    int p_w = width  + 2;
    unsigned char *padded =
        (unsigned char *)malloc((size_t)p_h * (size_t)p_w * sizeof(unsigned char));
    if (padded == NULL) {
        printf("[PROCESS] ERROR: No se pudo reservar memoria para la imagen padded.\n");
        free(sobel_rst);
        return metrics;
    }

    // Inicializar padding a 0
    memset(padded, 0, (size_t)p_h * (size_t)p_w * sizeof(unsigned char));

    // Copiar la imagen original en el centro de la imagen con padding
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            padded[(i + 1) * p_w + (j + 1)] = image_section[i * width + j];
        }
    }
    
    // Rellenar las filas superior e inferior con valores duplicados
    for (int j = 0; j < width; j++) {
        padded[0 * p_w + (j + 1)]            = padded[1 * p_w           + (j + 1)];
        padded[(height + 1) * p_w + (j + 1)] = padded[height * p_w      + (j + 1)];
    }
    
    // Rellenar las columnas izquierda y derecha con valores duplicados
    for (int i = 0; i < p_h; i++) {
        padded[i * p_w + 0]         = padded[i * p_w + 1];
        padded[i * p_w + (p_w - 1)] = padded[i * p_w + (p_w - 2)];
    }

    // 6. Crear y lanzar hilos para procesar la imagen en paralelo
    pthread_t   *threads = (pthread_t *)malloc(sizeof(pthread_t)   * num_threads);
    worker_args *args    = (worker_args *)malloc(sizeof(worker_args) * num_threads);
    if (threads == NULL || args == NULL) {
        printf("[PROCESS] ERROR: No se pudo reservar memoria para hilos.\n");
        free(threads);
        free(args);
        free(sobel_rst);
        free(padded);
        return metrics;
    }

    // Dividir el trabajo entre los hilos
    int base_rows = process_rows / num_threads;
    int remainder = process_rows % num_threads;
    int current_row = 0; 

    for (int t = 0; t < num_threads; t++) {
        int rows_for_thread = base_rows + (t < remainder ? 1 : 0);
        args[t].padded_image = padded;
        args[t].padded_width = p_w;
        args[t].height       = height;
        args[t].width        = width;
        args[t].sobel_mask_gx= sobel_mask_gx;
        args[t].sobel_mask_gy= sobel_mask_gy;
        args[t].thread_id    = t;
        args[t].start_row    = current_row;
        args[t].end_row      = current_row + rows_for_thread - 1;
        args[t].sobel_rst    = sobel_rst;

        current_row = args[t].end_row + 1;

        if (pthread_create(&threads[t], NULL, process_image_worker, &args[t]) != 0) {
            printf("[PROCESS] WARNING: fallo al crear hilo %d, ejecutando bloque en el hilo principal.\n", t);
            process_image_worker(&args[t]);
        }
    }

    // Esperar a que todos los hilos terminen
    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }

    // 7. NO guardamos aquí a disco: el resultado queda en el buffer global
    //    para que el esclavo/maestro decidan qué hacer con él.

    // 8. Liberar memoria temporal pero conservar sobel_rst
    free(threads);
    free(args);
    free(padded);

    // Reemplazar buffer global si ya había uno
    if (g_last_sobel) {
        free(g_last_sobel);
        g_last_sobel = NULL;
        g_last_h     = 0;
        g_last_w     = 0;
    }
    g_last_sobel = sobel_rst;
    g_last_h     = height;
    g_last_w     = width;

    // 1.4 Finalizar temporizador de procesamiento
    clock_t fin = clock();

    // 1.5 Medir el tiempo de procesamiento
    metrics.processing_t =
        ((double)(fin - start) / CLOCKS_PER_SEC);

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

// ----------------------------------------------------------------------
// Procesamiento de la imagen completa (modo standalone)
// ----------------------------------------------------------------------
metrics_node process(int *sobel_mask_gx,
                     int *sobel_mask_gy) {
    unsigned char *txt_image = NULL;
    int h = 0, w = 0;

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
    metrics = process_image(txt_image, h, w, sobel_mask_gx, sobel_mask_gy, net_latency);

    print_metrics(metrics);

    // Guardar el resultado global a disco:
    int out_h = 0, out_w = 0;
    if (get_last_sobel_dims(&out_h, &out_w)) {
        int *data = get_last_sobel_data();
        save_output_txt(data, out_h, out_w);
    }

    // Liberar memoria de la imagen leída
    free(txt_image);

    return metrics;
}

// ======================================================================
//  Helpers integrados
// ======================================================================

// Función para crear el directorio de archivos si no existe
void make_files_directory(void) {
    struct stat st = {0};

    if (stat(DIR_FILES, &st) == -1) {
        if (mkdir(DIR_FILES, 0777) == -1) {
            printf("[HELPER] ERROR: No se pudo crear el directorio %s: %s\n",
                   DIR_FILES, strerror(errno));
            return;
        }
    }
}

// Generar archivo de texto con matriz aleatoria
void make_random_matrix(int height, int width) {
    make_files_directory();

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s%s", DIR_FILES, INPUT_FILE);

    FILE *f = fopen(path, "w");
    if (f == NULL) {
        printf("[HELPER] ERROR: No se pudo abrir el archivo de prueba %s: %s\n",
               INPUT_FILE, strerror(errno));
        return;
    }

    srand((unsigned)time(NULL));
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            unsigned char valor = (unsigned char)(rand() % 256);
            fprintf(f, "%4d ", valor);
        }
        fprintf(f, "\n");
    }

    fclose(f);
}

// Guardar matriz de enteros en archivo de texto con nombre personalizado
void save_output_txt_as(const char *filename, int *matrix, int height, int width) {
    make_files_directory();

    char file_name[PATH_MAX];
    if (filename && filename[0]) {
        snprintf(file_name, sizeof(file_name), "%s%s", DIR_FILES, filename);
    } else {
        snprintf(file_name, sizeof(file_name), "%s%s", DIR_FILES, OUTPUT_FILE);
    }

    FILE *f = fopen(file_name, "w");
    if (f == NULL) {
        printf("[HELPER] ERROR: No se pudo abrir el archivo %s para escribir\n",
               file_name);
        return;
    }

    // Primera línea - dimensiones
    fprintf(f, "%d %d\n", height, width);

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            fprintf(f, "%d ", matrix[i * width + j]);
        }
        fprintf(f, "\n");
    }

    fclose(f);
}

// Versión antigua: escribe en OUTPUT_FILE por defecto
void save_output_txt(int *matrix, int height, int width) {
    save_output_txt_as(OUTPUT_FILE, matrix, height, width);
}

// Leer matriz desde archivo de texto
int read_input_txt(unsigned char **buffer, int *height, int *width) {
    char file_name[PATH_MAX];
    snprintf(file_name, sizeof(file_name), "%s%s", DIR_FILES, INPUT_FILE);

    FILE *f = fopen(file_name, "r");
    if (f == NULL) {
        printf("[HELPER] ERROR: No se pudo abrir el archivo de prueba %s: %s\n",
               file_name, strerror(errno));
        return 0;
    }

    int h = 0, w = 0;
    if (fscanf(f, "%d %d", &h, &w) != 2) {
        printf("[HELPER] ERROR: Formato invalido en cabecera de %s (se espera: height width)\n",
               file_name);
        fclose(f);
        return 0;
    }

    if (h <= 0 || w <= 0) {
        printf("[HELPER] ERROR: dimensiones invalidas en cabecera: %d x %d\n", h, w);
        fclose(f);
        return 0;
    }

    size_t needed = (size_t)h * (size_t)w;
    unsigned char *buf = (unsigned char *)malloc(needed);
    if (!buf) {
        printf("[HELPER] ERROR: Memoria insuficiente para leer matriz %dx%d\n", h, w);
        fclose(f);
        return 0;
    }

    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            int valor;
            int r = fscanf(f, "%d", &valor);
            if (r != 1) {
                if (feof(f)) {
                    printf("[HELPER] ERROR: EOF inesperado leyendo %s en posicion (%d,%d)\n",
                           file_name, i, j);
                } else {
                    printf("[HELPER] ERROR: fallo leyendo entero de %s en posicion (%d,%d): %s\n",
                           file_name, i, j, strerror(errno));
                }
                free(buf);
                fclose(f);
                return 0;
            }
            if (valor < 0)   valor = 0;
            if (valor > 255) valor = 255;
            buf[i * w + j] = (unsigned char)valor;
        }
    }

    fclose(f);

    *buffer = buf;
    *height = h;
    *width  = w;
    return 1;
}

// Mostrar matriz de unsigned char
void display_matrix(unsigned char *matriz, int height, int width) {
    for (int i = 0; i < height; i++) {
        printf("\t");
        for (int j = 0; j < width; j++) {
            printf("%3d ", matriz[i * width + j]);
        }
        printf("\n");
    }
}

// Mostrar matriz de int
void display_int_matrix(int *matriz, int height, int width) {
    for (int i = 0; i < height; i++) {
        printf("\t");
        for (int j = 0; j < width; j++) {
            printf("%4d ", matriz[i * width + j]);
        }
        printf("\n");
    }
}

// Cargar imagen JPEG a txt (grises)
int save_img_in_txt(const char *img_name) {
    if (!img_name) {
        printf("[HELPER] ERROR: argumento invalido para guardar imagen gris en matriz_test.\n");
        return 0;
    }

    make_files_directory();

    char ruta_imagen[PATH_MAX];
    snprintf(ruta_imagen, sizeof(ruta_imagen), "%s%s", DIR_FILES, img_name);

    FILE *infile = fopen(ruta_imagen, "rb");
    if (!infile) {
        printf("[HELPER] ERROR: No se pudo abrir la imagen %s: %s\n",
               ruta_imagen, strerror(errno));
        return 0;
    }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr         jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        printf("[HELPER] ERROR: Cabecera JPEG invalida para %s\n", ruta_imagen);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }

    jpeg_start_decompress(&cinfo);

    int src_w   = (int)cinfo.output_width;
    int src_h   = (int)cinfo.output_height;
    int src_comp = (int)cinfo.output_components;

    size_t row_stride = (size_t)src_w * (size_t)src_comp;
    JSAMPARRAY buffer =
        (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE,
                                   (JDIMENSION)row_stride, 1);

    unsigned char *gray_src = (unsigned char *)malloc((size_t)src_w * (size_t)src_h);
    if (!gray_src) {
        printf("[HELPER] ERROR: Memoria insuficiente para imagen (%dx%d).\n",
               src_w, src_h);
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }

    int y = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        unsigned char *row = buffer[0];
        if (src_comp == 3) {
            for (int x = 0; x < src_w; x++) {
                unsigned char r = row[x * 3 + 0];
                unsigned char g = row[x * 3 + 1];
                unsigned char b = row[x * 3 + 2];
                unsigned char lum =
                    (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);
                gray_src[y * src_w + x] = lum;
            }
        } else if (src_comp == 1) {
            memcpy(&gray_src[y * src_w], row, (size_t)src_w);
        } else {
            for (int x = 0; x < src_w; x++) {
                unsigned char r = row[x * src_comp + 0];
                unsigned char g = row[x * src_comp + (src_comp > 1 ? 1 : 0)];
                unsigned char b = row[x * src_comp + (src_comp > 2 ? 2 : 0)];
                unsigned char lum =
                    (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);
                gray_src[y * src_w + x] = lum;
            }
        }
        y++;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    char ruta_salida[PATH_MAX];
    snprintf(ruta_salida, sizeof(ruta_salida), "%s%s", DIR_FILES, INPUT_FILE);

    FILE *out = fopen(ruta_salida, "w");
    if (!out) {
        printf("[HELPER] ERROR: No se pudo crear %s: %s\n",
               ruta_salida, strerror(errno));
        free(gray_src);
        return 0;
    }

    fprintf(out, "%d %d\n", src_h, src_w);

    for (int i = 0; i < src_h; i++) {
        for (int j = 0; j < src_w; j++) {
            fprintf(out, "%d ", (int)gray_src[i * src_w + j]);
        }
        fprintf(out, "\n");
    }

    fclose(out);
    free(gray_src);

    return 1;
}

// Convertir archivo txt (matriz) a JPG en grises
int convert_txt_to_jpg(const char *img_name) {
    make_files_directory();

    char ruta_txt[PATH_MAX];
    snprintf(ruta_txt, sizeof(ruta_txt), "%s%s", DIR_FILES, OUTPUT_FILE);

    FILE *f = fopen(ruta_txt, "r");
    if (!f) {
        printf("[HELPER] ERROR: No se pudo abrir %s: %s\n",
               ruta_txt, strerror(errno));
        return 0;
    }

    int rows = 0, cols = 0;
    if (fscanf(f, "%d %d", &rows, &cols) != 2) {
        printf("[HELPER] ERROR: formato invalido en cabecera de %s (se espera: height width)\n",
               ruta_txt);
        fclose(f);
        return 0;
    }

    if (rows <= 0 || cols <= 0) {
        printf("[HELPER] ERROR: dimensiones invalidas en cabecera: %d x %d\n",
               rows, cols);
        fclose(f);
        return 0;
    }

    int *vals = (int *)malloc((size_t)rows * (size_t)cols * sizeof(int));
    if (!vals) {
        printf("[HELPER] ERROR: Memoria insuficiente.\n");
        fclose(f);
        return 0;
    }

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            long v;
            if (fscanf(f, "%ld", &v) != 1) {
                printf("[HELPER] ERROR: fallo leyendo valor en posicion (%d,%d)\n",
                       i, j);
                free(vals);
                fclose(f);
                return 0;
            }
            if (v < 0)   v = 0;
            if (v > 255) v = 255;
            vals[i * cols + j] = (int)v;
        }
    }

    fclose(f);

    unsigned char *buffer =
        (unsigned char *)malloc((size_t)rows * (size_t)cols);
    if (!buffer) {
        printf("[HELPER] ERROR: Memoria insuficiente para imagen %dx%d.\n",
               cols, rows);
        free(vals);
        return 0;
    }

    for (int i = 0; i < rows * cols; i++)
        buffer[i] = (unsigned char)vals[i];
    
    free(vals);

    const char *nombre_out = (img_name && img_name[0]) ? img_name : "result.jpg";
    char ruta_jpg[PATH_MAX];
    snprintf(ruta_jpg, sizeof(ruta_jpg), "%s%s", DIR_FILES, nombre_out);

    FILE *out = fopen(ruta_jpg, "wb");
    if (!out) {
        printf("[HELPER] ERROR: No se pudo crear %s: %s\n",
               ruta_jpg, strerror(errno));
        free(buffer);
        return 0;
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr        jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, out);

    cinfo.image_width      = (JDIMENSION)cols;
    cinfo.image_height     = (JDIMENSION)rows;
    cinfo.input_components = 1;
    cinfo.in_color_space   = JCS_GRAYSCALE;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 90, TRUE);

    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_pointer = &buffer[(size_t)cinfo.next_scanline * (size_t)cols];
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    fclose(out);
    free(buffer);
    return 1;
}

// ==========================
// Acceso al último resultado Sobel
// ==========================
int get_last_sobel_dims(int *height, int *width) {
    if (!g_last_sobel) return 0;
    if (height) *height = g_last_h;
    if (width)  *width  = g_last_w;
    return 1;
}

int *get_last_sobel_data(void) {
    return g_last_sobel;
}

void free_last_sobel(void) {
    if (g_last_sobel) {
        free(g_last_sobel);
        g_last_sobel = NULL;
    }
    g_last_h = 0;
    g_last_w = 0;
}
