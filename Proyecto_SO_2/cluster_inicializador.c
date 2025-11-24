#define _POSIX_C_SOURCE 200809L

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cluster.h"

// ---------------------------------------------------------------------
// Rank 0 lee desde stdin:
//   - Ruta de imagen
//   - Línea completa de máscaras Sobel en formato:
//       x=[-1,0,1,-2,0,2,-1,0,1] y=[1,2,1,0,0,0,-1,-2,-1]
// y luego las difunde vía MPI_Bcast a todos los ranks.
// ---------------------------------------------------------------------
// Parsea una lista de 9 enteros (se aceptan separadores ',' y espacios)
static int parse_vector9(const char *s, int out[9])
{
    if (!s)
        return -1;
    const char *p = s;
    int idx = 0;
    char *end;
    while (*p && idx < 9)
    {
        // buscar el inicio de un número
        while (*p && !((*p >= '0' && *p <= '9') || *p == '-'))
            p++;
        if (!*p)
            break;
        long v = strtol(p, &end, 10);
        if (p == end)
            break;
        out[idx++] = (int)v;
        p = end;
    }
    return (idx == 9) ? 0 : -1;
}

// Lee el archivo .config en el directorio actual. Formato esperado:
// image=path/to/img.jpg
// x=-1,0,1,-2,0,2,-1,0,1
// y=-1,-2,-1,0,0,0,1,2,1
static void leer_parametros_desde_config(const char *config_path,
                                         char *img_path, size_t img_sz,
                                         char *kernel_str, size_t kernel_sz)
{
    FILE *f = fopen(config_path, "r");
    if (!f)
    {
        fprintf(stderr, "[MASTER] No se encontró el archivo de configuración '%s'\n", config_path);
        exit(EXIT_FAILURE);
    }

    char line[512];
    char image_buf[512] = {0};
    int Kx[9];
    int Ky[9];
    int have_x = 0, have_y = 0, have_img = 0;

    while (fgets(line, sizeof(line), f))
    {
        // trim leading spaces
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0' || *p == '\n' || *p == '#')
            continue;
        // find '='
        char *eq = strchr(p, '=');
        if (!eq)
            continue;
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        // trim trailing newline
        val[strcspn(val, "\r\n")] = '\0';

        // key trim
        char *kend = key + strlen(key) - 1;
        while (kend > key && (*kend == ' ' || *kend == '\t'))
        {
            *kend = '\0';
            kend--;
        }

        if (strcmp(key, "image") == 0)
        {
            snprintf(image_buf, sizeof(image_buf), "%s", val);
            have_img = 1;
        }
        else if (strcmp(key, "x") == 0)
        {
            if (parse_vector9(val, Kx) == 0)
                have_x = 1;
        }
        else if (strcmp(key, "y") == 0)
        {
            if (parse_vector9(val, Ky) == 0)
                have_y = 1;
        }
    }
    fclose(f);

    if (!have_img)
    {
        fprintf(stderr, "[MASTER] .config no contiene 'image='\n");
        exit(EXIT_FAILURE);
    }
    if (!have_x || !have_y)
    {
        fprintf(stderr, "[MASTER] .config debe contener 'x=' y 'y=' con 9 valores cada una\n");
        exit(EXIT_FAILURE);
    }

    // preparar kernel_str en el formato esperado por el parser existente
    int o = snprintf(kernel_str, kernel_sz,
                     "x=[%d,%d,%d,%d,%d,%d,%d,%d,%d] y=[%d,%d,%d,%d,%d,%d,%d,%d,%d]",
                     Kx[0], Kx[1], Kx[2], Kx[3], Kx[4], Kx[5], Kx[6], Kx[7], Kx[8],
                     Ky[0], Ky[1], Ky[2], Ky[3], Ky[4], Ky[5], Ky[6], Ky[7], Ky[8]);

    if (o < 0)
    {
        fprintf(stderr, "[MASTER] Error construyendo kernel string\n");
        exit(EXIT_FAILURE);
    }
    if ((size_t)o >= kernel_sz)
    {
        fprintf(stderr, "[MASTER] Advertencia: kernel string truncado (longitud necesaria=%d, buf=%zu)\n", o, kernel_sz);
        // continuar con la versión truncada
    }

    snprintf(img_path, img_sz, "%s", image_buf);

    printf("\n[MASTER config] Imagen  : '%s'\n", img_path);
    printf("[MASTER config] Máscaras: '%s'\n\n", kernel_str);
}

int main(int argc, char **argv)
{
    int rank, size;

    // 1. Inicializar MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char img_path[512] = {0};
    char kernel_str[256] = {0};

    // 2. Solo el rank 0 lee los parámetros desde el archivo .config
    if (rank == 0)
    {
        const char *cfg = ".config";
        leer_parametros_desde_config(cfg, img_path, sizeof(img_path),
                                     kernel_str, sizeof(kernel_str));
    }

    // 3. Difundir parámetros a todos los procesos vía MPI
    MPI_Bcast(img_path, (int)sizeof(img_path), MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(kernel_str, (int)sizeof(kernel_str), MPI_CHAR, 0, MPI_COMM_WORLD);

    // 4. Ejecutar el clúster MPI con los parámetros recibidos
    int rc = cluster_run(img_path, kernel_str);

    // 5. Finalizar MPI
    MPI_Finalize();

    return rc;
}
