// main_cliente.c
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define CLIENT_PORT 7000

int main(void)
{
    char img_path[512];
    int gx[9];
    int gy[9];

    printf("=== Cliente del cluster Sobel ===\n\n");

    // 1. Pedir ruta de la imagen
    printf("Ruta de la imagen (jpg/png/...): ");
    if (!fgets(img_path, sizeof(img_path), stdin)) {
        fprintf(stderr, "Error leyendo ruta de imagen.\n");
        return 1;
    }
    // quitar salto de linea
    size_t len = strlen(img_path);
    if (len > 0 && img_path[len-1] == '\n') {
        img_path[len-1] = '\0';
    }

    // 2. Pedir coeficientes de Sobel X
    printf("\nIngrese 9 coeficientes para Sobel X (Gx), separados por espacios.\n");
    printf("Ejemplo clasico: -1 0 1 -2 0 2 -1 0 1\n> ");
    for (int i = 0; i < 9; i++) {
        if (scanf("%d", &gx[i]) != 1) {
            fprintf(stderr, "Error leyendo coeficientes de Sobel X.\n");
            return 1;
        }
    }

    // 3. Pedir coeficientes de Sobel Y
    printf("\nIngrese 9 coeficientes para Sobel Y (Gy), separados por espacios.\n");
    printf("Ejemplo clasico: 1 2 1 0 0 0 -1 -2 -1\n> ");
    for (int i = 0; i < 9; i++) {
        if (scanf("%d", &gy[i]) != 1) {
            fprintf(stderr, "Error leyendo coeficientes de Sobel Y.\n");
            return 1;
        }
    }

    // Limpiar resto de la linea (por si quedó basura en stdin)
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }

    // 4. Construir la segunda línea en formato:
    //    x=[a,b,c,d,e,f,g,h,i] y=[j,k,l,m,n,o,p,q,r]
    char mask_line[1024];
    int offset = 0;

    offset += snprintf(mask_line + offset, sizeof(mask_line) - offset, "x=[");
    for (int i = 0; i < 9; i++) {
        offset += snprintf(mask_line + offset, sizeof(mask_line) - offset,
                           (i < 8) ? "%d," : "%d", gx[i]);
    }
    offset += snprintf(mask_line + offset, sizeof(mask_line) - offset, "] y=[");
    for (int i = 0; i < 9; i++) {
        offset += snprintf(mask_line + offset, sizeof(mask_line) - offset,
                           (i < 8) ? "%d," : "%d", gy[i]);
    }
    offset += snprintf(mask_line + offset, sizeof(mask_line) - offset, "]");

    printf("\n[CLIENTE] Enviando al cluster:\n");
    printf("   Imagen : %s\n", img_path);
    printf("   Mascara: %s\n", mask_line);

    // 5. Conectarse al servidor (master MPI) en localhost:7000
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[CLIENTE] socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(CLIENT_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[CLIENTE] connect");
        close(sock);
        return 1;
    }

    // 6. Enviar las dos líneas:
    //    <ruta_imagen>\n
    //    x=[...] y=[...]\n
    char line1[600];
    snprintf(line1, sizeof(line1), "%s\n", img_path);

    char line2[1100];
    snprintf(line2, sizeof(line2), "%s\n", mask_line);

    ssize_t n1 = send(sock, line1, strlen(line1), 0);
    if (n1 < 0) {
        perror("[CLIENTE] send line1");
        close(sock);
        return 1;
    }

    ssize_t n2 = send(sock, line2, strlen(line2), 0);
    if (n2 < 0) {
        perror("[CLIENTE] send line2");
        close(sock);
        return 1;
    }

    printf("\n[CLIENTE] Parametros enviados. Puedes observar la ejecucion del cluster.\n");

    close(sock);
    return 0;
}
