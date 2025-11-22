#define _POSIX_C_SOURCE 200809L

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_FAILURE_STRINGS
#include "stb_image.h"

#include "cluster.h"
#include "process.h"
#include "histogram_lib.h"

// ----------------- Macros/constantes -----------------
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define TAG_META 10
#define TAG_DATA 11
#define TAG_DONE 12
#define TAG_RESULT 13 // Resultado Sobel (int) de cada worker

#define OUTPUT_DIR "./output"

// ----------------- Utils SO -----------------
void ensure_dir(const char *path)
{
    if (!path || !*path)
        return;
    struct stat st;
    if (stat(path, &st) == 0)
    {
        if (!S_ISDIR(st.st_mode))
        {
            fprintf(stderr, "[ERR] '%s' existe pero no es directorio\n", path);
            exit(EXIT_FAILURE);
        }
        return;
    }
    if (mkdir(path, 0775) != 0 && errno != EEXIST)
    {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }
}

// ----------------- Parser de kernel "[e1, ..., e9]" -----------------
int parse_kernel9(const char *s, int K[3][3])
{
    if (!s)
        return -1;
    const char *p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p != '[')
        return -1;
    p++;
    for (int i = 0; i < 9; i++)
    {
        while (*p == ' ' || *p == '\t')
            p++;
        char *end = NULL;
        long v = strtol(p, &end, 10);
        if (p == end)
            return -1;
        K[i / 3][i % 3] = (int)v;
        p = end;
        while (*p == ' ' || *p == '\t')
            p++;
        if (i < 8)
        {
            if (*p != ',')
                return -1;
            p++;
        }
    }
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p != ']')
        return -1;
    return 0;
}

/*
 * parse_sobel_xy
 * Espera un string del estilo:
 *
 *   x=[a,b,c,d,e,f,g,h,i] y=[j,k,l,m,n,o,p,q,r]
 *
 * y llena Kx y Ky con los 9 coeficientes de cada máscara.
 */
static int parse_sobel_xy(const char *s,
                          int Kx[3][3],
                          int Ky[3][3])
{
    if (!s)
        return -1;

    const char *px = strstr(s, "x=");
    if (!px)
        px = strstr(s, "X=");
    const char *py = strstr(s, "y=");
    if (!py)
        py = strstr(s, "Y=");
    if (!px || !py)
    {
        fprintf(stderr, "[MASTER] No se encontraron prefijos x= / y= en la mascara.\n");
        return -1;
    }

    const char *sx = strchr(px, '[');
    const char *sy = strchr(py, '[');
    if (!sx || !sy)
    {
        fprintf(stderr, "[MASTER] Falta '[' en definicion de x o y.\n");
        return -1;
    }

    const char *ex = strchr(sx, ']');
    const char *ey = strchr(sy, ']');
    if (!ex || !ey)
    {
        fprintf(stderr, "[MASTER] Falta ']' en definicion de x o y.\n");
        return -1;
    }

    char bufX[256];
    char bufY[256];

    size_t lenx = (size_t)(ex - sx + 1);
    size_t leny = (size_t)(ey - sy + 1);

    if (lenx >= sizeof(bufX) || leny >= sizeof(bufY))
    {
        fprintf(stderr, "[MASTER] Linea de mascara demasiado larga.\n");
        return -1;
    }

    memcpy(bufX, sx, lenx);
    bufX[lenx] = '\0';
    memcpy(bufY, sy, leny);
    bufY[leny] = '\0';

    if (parse_kernel9(bufX, Kx) != 0)
    {
        fprintf(stderr, "[MASTER] Error parseando kernel X.\n");
        return -1;
    }
    if (parse_kernel9(bufY, Ky) != 0)
    {
        fprintf(stderr, "[MASTER] Error parseando kernel Y.\n");
        return -1;
    }

    return 0;
}

// ----------------- Carga universal a matriz gris -----------------
int load_any_to_gray(const char *path, uint8_t **gray_out, int *W, int *H)
{
    if (!path || !gray_out || !W || !H)
        return -1;
    int w = 0, h = 0, nc = 0;
    unsigned char *pix = stbi_load(path, &w, &h, &nc, 0); // auto-detecta canales
    if (!pix)
    {
        fprintf(stderr, "[ERR] stbi_load fallo para '%s'\n", path);
        return -2;
    }
    if (w < 3 || h < 3)
    {
        stbi_image_free(pix);
        fprintf(stderr, "[ERR] imagen muy pequena (%dx%d)\n", w, h);
        return -3;
    }

    uint8_t *gray = (uint8_t *)malloc((size_t)w * h);
    if (!gray)
    {
        stbi_image_free(pix);
        fprintf(stderr, "[ERR] sin memoria para %dx%d\n", w, h);
        return -4;
    }

    const size_t N = (size_t)w * h;
    if (nc == 1 || nc == 2)
    {
        for (size_t i = 0; i < N; ++i)
            gray[i] = pix[i * nc + 0];
    }
    else if (nc == 3 || nc == 4)
    {
        for (size_t i = 0; i < N; ++i)
        {
            uint8_t r = pix[i * nc + 0];
            uint8_t g = pix[i * nc + 1];
            uint8_t b = pix[i * nc + 2];
            int y = (int)(0.299f * r + 0.587f * g + 0.114f * b + 0.5f);
            gray[i] = (uint8_t)(y < 0 ? 0 : (y > 255 ? 255 : y));
        }
    }
    else
    {
        stbi_image_free(pix);
        free(gray);
        fprintf(stderr, "[ERR] canales no soportados: %d\n", nc);
        return -5;
    }

    stbi_image_free(pix);
    *gray_out = gray;
    *W = w;
    *H = h;
    return 0;
}

// ----------------- Matrices: guardar CSV/BIN -----------------
int write_u8_matrix_csv(const char *path, const uint8_t *m, int W, int H)
{
    FILE *f = fopen(path, "w");
    if (!f)
    {
        perror("fopen csv");
        return -1;
    }
    for (int y = 0; y < H; ++y)
    {
        for (int x = 0; x < W; ++x)
        {
            if (x)
                fputc(',', f);
            fprintf(f, "%u", (unsigned)m[(size_t)y * W + x]);
        }
        fputc('\n', f);
    }
    fclose(f);
    return 0;
}

int write_u8_matrix_bin(const char *path, const uint8_t *m, int W, int H)
{
    FILE *f = fopen(path, "wb");
    if (!f)
    {
        perror("fopen bin");
        return -1;
    }
    size_t n = (size_t)W * H;
    if (fwrite(m, 1, n, f) != n)
    {
        fprintf(stderr, "[ERR] fwrite bin\n");
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

// ----------------- Histograma (para Sobel) -----------------
void histogram_u8(const uint8_t *img, int W, int H, uint64_t hist[256])
{
    memset(hist, 0, 256 * sizeof(uint64_t));
    size_t N = (size_t)W * H;
    for (size_t i = 0; i < N; i++)
        hist[img[i]]++;
}

int write_hist_csv(const char *fname, const uint64_t hist[256])
{
    FILE *f = fopen(fname, "w");
    if (!f)
    {
        perror("fopen hist.csv");
        return -1;
    }
    fprintf(f, "valor,conteo\n");
    for (int i = 0; i < 256; i++)
        fprintf(f, "%d,%" PRIu64 "\n", i, hist[i]);
    fclose(f);
    return 0;
}

// ---------------------------------------------------------------------
// Helper: leer archivo de texto completo en memoria
// ---------------------------------------------------------------------
static char *read_text_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        perror("[MASTER] fopen sobel_full.txt");
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0)
    {
        perror("[MASTER] fseek");
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0)
    {
        perror("[MASTER] ftell");
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf)
    {
        fprintf(stderr, "[MASTER] sin memoria para leer sobel_full.txt\n");
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

// =======================
// cluster_run: maestro + esclavos usando process_image()
// =======================
int cluster_run(const char *img_path, const char *kernel_str)
{
    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2)
    {
        if (rank == 0)
        {
            fprintf(stderr, "[CLUSTER] Se requieren al menos 2 procesos (1 maestro + 1 esclavo).\n");
        }
        return EXIT_FAILURE;
    }

    // =========================================================
    // MAESTRO (rank 0)
    // =========================================================
    if (rank == 0)
    {
        ensure_dir(OUTPUT_DIR);

        int Kx[3][3];
        int Ky[3][3];
        int sobel_gx[9];
        int sobel_gy[9];

        // --- 1. Usar directamente los parámetros recibidos ---
        if (!img_path || !kernel_str || img_path[0] == '\0' || kernel_str[0] == '\0')
        {
            fprintf(stderr, "[MASTER] Parametros img_path/kernel_str invalidos.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        printf("[MASTER] Usando imagen: '%s'\n", img_path);
        printf("[MASTER] Usando mascara Sobel X/Y: '%s'\n", kernel_str);

        if (parse_sobel_xy(kernel_str, Kx, Ky) != 0)
        {
            fprintf(stderr, "[MASTER] Formato de mascara Sobel X/Y invalido.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                sobel_gx[i * 3 + j] = Kx[i][j];
                sobel_gy[i * 3 + j] = Ky[i][j];
            }
        }

        // --- 2. Cargar imagen en gris ---
        uint8_t *gray = NULL;
        int W = 0, H = 0;
        int rc = load_any_to_gray(img_path, &gray, &W, &H);
        if (rc != 0)
        {
            fprintf(stderr, "[MASTER] Error cargando imagen '%s' (rc=%d)\n", img_path, rc);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        printf("[MASTER] Imagen cargada: %dx%d\n", W, H);

        // Guardar copia de la imagen de entrada
        char in_csv[512];
        snprintf(in_csv, sizeof(in_csv), "%s/input_gray.csv", OUTPUT_DIR);
        write_u8_matrix_csv(in_csv, gray, W, H);

        // --- 3. Distribuir trabajo entre esclavos ---
        int workers = size - 1;
        if (workers > 3)
            workers = 3;
        if (workers > H)
            workers = H; // no más esclavos que filas

        printf("[MASTER] Trabajadores usados: %d (de %d procesos totales)\n", workers, size);

        int base_rows = H / workers;
        int rem_rows = H % workers;

        // Arrays para reconstruir la imagen Sobel completa
        int *rows_for_worker = (int *)malloc((workers + 1) * sizeof(int));
        int *start_for_worker = (int *)malloc((workers + 1) * sizeof(int));
        if (!rows_for_worker || !start_for_worker)
        {
            fprintf(stderr, "[MASTER] sin memoria para arrays de worker.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        int *sobel_full = (int *)malloc((size_t)W * (size_t)H * sizeof(int));
        if (!sobel_full)
        {
            fprintf(stderr, "[MASTER] sin memoria para sobel_full.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        // Enviar trabajo a cada worker
        for (int w = 1; w <= workers; ++w)
        {
            int rows = base_rows + ((w - 1) < rem_rows ? 1 : 0);
            int start_row = (base_rows * (w - 1)) + MIN(w - 1, rem_rows);

            rows_for_worker[w] = rows;
            start_for_worker[w] = start_row;

            // Meta: [W, rows]
            int meta[2] = {W, rows};

            // 3.1 Enviar meta y kernels
            MPI_Send(meta, 2, MPI_INT, w, TAG_META, MPI_COMM_WORLD);
            MPI_Send(sobel_gx, 9, MPI_INT, w, TAG_META, MPI_COMM_WORLD);
            MPI_Send(sobel_gy, 9, MPI_INT, w, TAG_META, MPI_COMM_WORLD);

            // 3.2 Enviar subimagen midiendo latencia REAL con MPI_Wtime()
            size_t n = (size_t)rows * (size_t)W;
            double t0 = MPI_Wtime();
            MPI_Send(&gray[start_row * W], (int)n, MPI_UNSIGNED_CHAR,
                     w, TAG_DATA, MPI_COMM_WORLD);
            double t1 = MPI_Wtime();
            double net_latency = t1 - t0;

            // 3.3 Enviar al worker la latencia medida
            MPI_Send(&net_latency, 1, MPI_DOUBLE, w, TAG_META, MPI_COMM_WORLD);

            printf("[MASTER] Enviado bloque filas [%d..%d] al worker %d (lat=%.6f s)\n",
                   start_row, start_row + rows - 1, w, net_latency);
        }

        // Los procesos extra (si size-1 > workers) reciben un mensaje vacío
        for (int w = workers + 1; w < size; ++w)
        {
            int meta[2] = {-1, 0}; // W=-1 => no trabajo
            double dummy_lat = 0.0;
            int zeros[9] = {0};

            MPI_Send(meta, 2, MPI_INT, w, TAG_META, MPI_COMM_WORLD);
            MPI_Send(zeros, 9, MPI_INT, w, TAG_META, MPI_COMM_WORLD);
            MPI_Send(zeros, 9, MPI_INT, w, TAG_META, MPI_COMM_WORLD);
            MPI_Send(&dummy_lat, 1, MPI_DOUBLE, w, TAG_META, MPI_COMM_WORLD);
        }

        // --- 4. Recibir métricas y resultados de cada worker ---
        for (int w = 1; w <= workers; ++w)
        {
            double pack[4];
            MPI_Status st;

            // Primero las métricas
            MPI_Recv(pack, 4, MPI_DOUBLE, w, TAG_DONE, MPI_COMM_WORLD, &st);

            metrics_node m;
            m.processing_t = pack[0];
            m.network_latency_t = pack[1];
            m.data_transferred = pack[2];
            m.throughput = pack[3];

            printf("\n[MASTER] Métricas worker %d:\n", w);
            print_metrics(m);

            // Luego el bloque Sobel
            int rows = rows_for_worker[w];
            int start_row = start_for_worker[w];
            size_t n = (size_t)rows * (size_t)W;

            MPI_Recv(&sobel_full[start_row * W],
                     (int)n,
                     MPI_INT,
                     w, // fuente: worker w
                     TAG_RESULT,
                     MPI_COMM_WORLD,
                     &st);

            printf("[MASTER] Recibido bloque Sobel filas [%d..%d] desde worker %d\n",
                   start_row, start_row + rows - 1, w);
        }

        // --- 5. Guardar la imagen Sobel completa a disco ---
        // Esto genera un txt en ../files/sobel_full.txt con el formato esperado
        save_output_txt_as("sobel_full.txt", sobel_full, H, W);

        // --- 6. Generar histograma sobre la imagen Sobel y conectarlo a la LCD ---
        char sobel_path[PATH_MAX];
        snprintf(sobel_path, sizeof(sobel_path), "%s%s", DIR_FILES, "sobel_full.txt");

        char *sobel_text = read_text_file(sobel_path);
        if (!sobel_text)
        {
            fprintf(stderr, "[MASTER] No se pudo leer sobel_full.txt para histograma.\n");
        }
        else
        {
            int hist_full[256];
            if (generar_histograma(sobel_text, hist_full) != 0)
            {
                fprintf(stderr, "[MASTER] Error al generar histograma de Sobel.\n");
            }
            else
            {
                // 6.1 Mostrar histograma en LCD usando la biblioteca + driver
                histogram_show(sobel_text);
                int index = 0;
                for (int i = 0; i < LCD_BINS; ++i)
                {
                    // Recorremos automáticamente los 16 grupos
                    move_auto(&index);
                }

                // 6.2 Guardar también CSV por si el profe quiere ver
                uint64_t hist64[256];
                for (int i = 0; i < 256; ++i)
                {
                    hist64[i] = (hist_full[i] < 0) ? 0 : (uint64_t)hist_full[i];
                }

                char hist_csv[512];
                snprintf(hist_csv, sizeof(hist_csv), "%s/histogram_sobel.csv", OUTPUT_DIR);
                write_hist_csv(hist_csv, hist64);

                printf("[MASTER] Histograma Sobel guardado en %s\n", hist_csv);
            }

            free(sobel_text);
        }

        free(sobel_full);
        free(rows_for_worker);
        free(start_for_worker);
        free(gray);

        return 0;
    }

    // =========================================================
    // ESCLAVOS (rank > 0)
    // =========================================================
    int meta[2];
    MPI_Status st;

    // Recibir meta (W, rows) y kernels
    MPI_Recv(meta, 2, MPI_INT, 0, TAG_META, MPI_COMM_WORLD, &st);

    int W = meta[0];
    int rows = meta[1];

    int sobel_gx[9];
    int sobel_gy[9];
    MPI_Recv(sobel_gx, 9, MPI_INT, 0, TAG_META, MPI_COMM_WORLD, &st);
    MPI_Recv(sobel_gy, 9, MPI_INT, 0, TAG_META, MPI_COMM_WORLD, &st);

    size_t n = (size_t)W * (size_t)rows;
    unsigned char *section = (unsigned char *)malloc(n);
    if (!section)
    {
        fprintf(stderr, "[WORKER %d] ERROR: sin memoria para %zu bytes\n", rank, n);
        double err[4] = {-1, -1, -1, -1};
        MPI_Send(err, 4, MPI_DOUBLE, 0, TAG_DONE, MPI_COMM_WORLD);
        return 0;
    }

    // Declarar la variable de latencia antes de usarla
    double net_latency = 0.0;

    // primero recibe la imagen
    MPI_Recv(section, (int)n, MPI_UNSIGNED_CHAR, 0, TAG_DATA, MPI_COMM_WORLD, &st);

    // Luego recibir la latencia medida
    MPI_Recv(&net_latency, 1, MPI_DOUBLE, 0, TAG_META, MPI_COMM_WORLD, &st);

    printf("[WORKER %d] Recibido bloque %dx%d (lat=%.6f s)\n",
           rank, W, rows, net_latency);

    // Si W <= 0 significa "no hay trabajo" para este proceso
    if (W <= 0 || rows <= 0)
    {
        double zeros[4] = {0, 0, 0, 0};
        MPI_Send(zeros, 4, MPI_DOUBLE, 0, TAG_DONE, MPI_COMM_WORLD);
        return 0;
    }

    // ---- Usar lógica de procesamiento (process_image) con Gx y Gy ----
    metrics_node m = process_image(section, rows, W, sobel_gx, sobel_gy, net_latency);

    // Obtener el resultado Sobel local
    int out_h = 0, out_w = 0;
    int ok_dims = get_last_sobel_dims(&out_h, &out_w);
    int *sobel_local = get_last_sobel_data();

    if (!ok_dims || !sobel_local || out_h != rows || out_w != W)
    {
        fprintf(stderr, "[WORKER %d] ERROR: dimensiones Sobel inesperadas.\n", rank);
        double err[4] = {-1, -1, -1, -1};
        MPI_Send(err, 4, MPI_DOUBLE, 0, TAG_DONE, MPI_COMM_WORLD);
        free(section);
        free_last_sobel();
        return 0;
    }

    // Guardar porción procesada a disco con nombre único
    // Ej: ../files/output_rankX.txt
    char fname[64];
    snprintf(fname, sizeof(fname), "output_rank%d.txt", rank);
    save_output_txt_as(fname, sobel_local, out_h, out_w);

    // Empaquetar métricas en 4 doubles y enviarlas
    double pack[4] = {
        m.processing_t,
        m.network_latency_t,
        m.data_transferred,
        m.throughput};

    MPI_Send(pack, 4, MPI_DOUBLE, 0, TAG_DONE, MPI_COMM_WORLD);

    // Enviar también los datos Sobel al maestro
    MPI_Send(sobel_local, out_h * out_w, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD);

    free(section);
    free_last_sobel(); // liberar buffer local

    return 0;
}
