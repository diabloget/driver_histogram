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
static void leer_parametros_desde_stdin(char *img_path, size_t img_sz,
                                        char *kernel_str, size_t kernel_sz)
{
    char buf_img[512];
    char buf_kernel[512];

    printf("\n=== Configuración del clúster Sobel ===\n");

    // Ruta de la imagen
    printf("Ruta de la imagen (ej: ../files/foto.jpg): ");
    fflush(stdout);
    if (!fgets(buf_img, sizeof(buf_img), stdin)) {
        fprintf(stderr, "Error leyendo ruta de imagen por stdin.\n");
        exit(EXIT_FAILURE);
    }
    // Quitar '\n'
    buf_img[strcspn(buf_img, "\r\n")] = '\0';

    if (buf_img[0] == '\0') {
        fprintf(stderr, "Ruta de imagen vacía.\n");
        exit(EXIT_FAILURE);
    }

    // Línea completa de máscaras en formato x=[...] y=[...]
    printf("Máscaras Sobel en formato x=[...] y=[...]\n");
    printf("Ejemplo: x=[-1,0,1,-2,0,2,-1,0,1] y=[1,2,1,0,0,0,-1,-2,-1]\n");
    printf("Ingrese la línea: ");
    fflush(stdout);
    if (!fgets(buf_kernel, sizeof(buf_kernel), stdin)) {
        fprintf(stderr, "Error leyendo máscara Sobel por stdin.\n");
        exit(EXIT_FAILURE);
    }
    buf_kernel[strcspn(buf_kernel, "\r\n")] = '\0';

    if (buf_kernel[0] == '\0') {
        fprintf(stderr, "Máscara Sobel vacía.\n");
        exit(EXIT_FAILURE);
    }

    // Copiar a buffers de salida
    strncpy(img_path, buf_img, img_sz - 1);
    img_path[img_sz - 1] = '\0';

    strncpy(kernel_str, buf_kernel, kernel_sz - 1);
    kernel_str[kernel_sz - 1] = '\0';

    printf("\n[MASTER stdin] Imagen  : '%s'\n", img_path);
    printf("[MASTER stdin] Máscaras: '%s'\n\n", kernel_str);
}

int main(int argc, char **argv)
{
    int rank, size;

    // 1. Inicializar MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char img_path[512]   = {0};
    char kernel_str[256] = {0};

    // 2. Solo el rank 0 pregunta al usuario por stdin
    if (rank == 0) {
        leer_parametros_desde_stdin(img_path, sizeof(img_path),
                                    kernel_str, sizeof(kernel_str));
    }

    // 3. Difundir parámetros a todos los procesos vía MPI 
    MPI_Bcast(img_path,   (int)sizeof(img_path),   MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(kernel_str, (int)sizeof(kernel_str), MPI_CHAR, 0, MPI_COMM_WORLD);

    // 4. Ejecutar el clúster MPI con los parámetros recibidos
    int rc = cluster_run(img_path, kernel_str);

    // 5. Finalizar MPI
    MPI_Finalize();

    return rc;
}
