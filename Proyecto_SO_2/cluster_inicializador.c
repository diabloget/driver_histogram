#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>         // close()
#include <arpa/inet.h>      // socket, bind, listen, accept, inet_pton
#include <errno.h>

#include <mpi.h>

#include "cluster.h"

// =========================================================
// CONFIGURACIÓN DEL DISPOSITIVO EXTERNO (ESP32 / Raspberry)
// =========================================================

// --- ESP32 (por defecto) ---
static const char *EXT_IP   = "192.168.4.1";   // Cambiar según tu ESP32
static const int   EXT_PORT = 5000;

// --- Raspberry (ejemplo) ---
// static const char *EXT_IP   = "192.168.1.50";
// static const int   EXT_PORT = 6000;

// =========================================================
// SOCKET GLOBAL HACIA DISPOSITIVO EXTERNO (ESP32/Raspberry)
// =========================================================
static int g_fd = -1;

// =========================================================
// CONFIGURACIÓN DEL SERVIDOR PARA EL CLIENTE LOCAL
// =========================================================
static const int CLIENT_PORT = 7000;  // puerto donde se conecta el cliente

// =========================================================
// Inicializar conexión TCP real con el dispositivo externo
// =========================================================
static void init_external_connection(void)
{
    printf("[INIT] Conectando al dispositivo externo %s:%d...\n", EXT_IP, EXT_PORT);

    g_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_fd < 0) {
        perror("[INIT] socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(EXT_PORT);

    if (inet_pton(AF_INET, EXT_IP, &addr.sin_addr) <= 0) {
        perror("[INIT] inet_pton");
        close(g_fd);
        exit(EXIT_FAILURE);
    }

    if (connect(g_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[INIT] connect");
        close(g_fd);
        exit(EXIT_FAILURE);
    }

    printf("[INIT] Conexión TCP con dispositivo externo establecida correctamente.\n");
}

// =========================================================
// IMPLEMENTACIÓN REAL DE histdev_write() (usada en cluster.c)
// =========================================================
int histdev_write(const uint8_t *data, size_t len)
{
    if (g_fd < 0) {
        fprintf(stderr, "[histdev_write] ERROR: socket no inicializado.\n");
        return -1;
    }

    ssize_t sent = send(g_fd, data, len, 0);
    if (sent < 0) {
        perror("[histdev_write] send");
        return -1;
    }

    return 0;
}

// =========================================================
// Leer dos líneas (imagen y mascara) desde el socket del cliente
// Formato esperado:
//   <ruta_imagen>\n
//   <mascara>\n
// =========================================================
static int recv_client_params(int client_fd,
                              char *img_path, size_t img_path_sz,
                              char *kernel_str, size_t kernel_sz)
{
    char buf[1024];
    int pos = 0;
    int newlines = 0;

    while (pos < (int)sizeof(buf) - 1 && newlines < 2) {
        ssize_t r = recv(client_fd, &buf[pos], 1, 0);
        if (r <= 0) {
            perror("[SERVER] recv");
            return -1;
        }
        if (buf[pos] == '\n') {
            newlines++;
        }
        pos++;
    }
    buf[pos] = '\0';

    char *line1 = buf;
    char *line2 = strchr(line1, '\n');
    if (line2) {
        *line2 = '\0';
        line2++;
    } else {
        fprintf(stderr, "[SERVER] Formato invalido (no hay segunda linea)\n");
        return -1;
    }
    char *line3 = strchr(line2, '\n');
    if (line3) {
        *line3 = '\0';
    }

    strncpy(img_path, line1, img_path_sz - 1);
    img_path[img_path_sz - 1] = '\0';

    strncpy(kernel_str, line2, kernel_sz - 1);
    kernel_str[kernel_sz - 1] = '\0';

    printf("[SERVER] Recibido del cliente:\n");
    printf("         Imagen : '%s'\n", img_path);
    printf("         Mascara: '%s'\n", kernel_str);

    return 0;
}

// =========================================================
// main real: inicializa MPI, espera cliente y corre el clúster
// =========================================================
int main(int argc, char **argv)
{
    int rank, size;

    // 1. Inicializar MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char img_path[512]   = {0};
    char kernel_str[256] = {0};

    int server_fd  = -1;
    int client_fd  = -1;

    if (rank == 0) {
        // 2. Inicializar conexión con ESP32/Raspberry
        init_external_connection();

        // 3. Levantar servidor para el cliente local
        printf("[SERVER] Iniciando servidor de parametros en puerto %d...\n", CLIENT_PORT);

        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            perror("[SERVER] socket");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("[SERVER] setsockopt");
            // no abortamos, pero lo reportamos
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port        = htons(CLIENT_PORT);

        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("[SERVER] bind");
            close(server_fd);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        if (listen(server_fd, 1) < 0) {
            perror("[SERVER] listen");
            close(server_fd);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        printf("[SERVER] Esperando conexión de cliente...\n");

        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("[SERVER] accept");
            close(server_fd);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        printf("[SERVER] Cliente conectado.\n");

        if (recv_client_params(client_fd,
                               img_path, sizeof(img_path),
                               kernel_str, sizeof(kernel_str)) != 0) {
            fprintf(stderr, "[SERVER] Error recibiendo parametros del cliente.\n");
            close(client_fd);
            close(server_fd);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        close(client_fd);
        close(server_fd);
        client_fd = -1;
        server_fd = -1;
    }

    // 4. Difundir parametros a todos los procesos
    MPI_Bcast(img_path,   (int)sizeof(img_path),   MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(kernel_str, (int)sizeof(kernel_str), MPI_CHAR, 0, MPI_COMM_WORLD);

    // 5. Ejecutar el clúster MPI con los parámetros recibidos
    int rc = cluster_run(img_path, kernel_str);

    // 6. Cerrar socket al dispositivo externo en el maestro
    if (rank == 0 && g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }

    // 7. Finalizar MPI
    MPI_Finalize();

    return rc;
}
