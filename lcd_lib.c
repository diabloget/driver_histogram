#include "lcd_lib.h"
#include <stdio.h>      // Para snprintf, perror
#include <string.h>     // Para strlen, strcmp
#include <fcntl.h>      // Para open, O_WRONLY, O_RDONLY
#include <unistd.h>     // Para write, read, close
#include <errno.h>      // Para errno

/**
 * @brief Función helper INTERNA para enviar un comando (string) al driver.
 */
static int send_command(const char* cmd_string) {
    // Esta función no cambia. Lee el DRIVER_NODE de lcd_lib.h
    int fd = open(DRIVER_NODE, O_WRONLY);
    if (fd < 0) {
        perror("liblcd (send_command): Error al abrir el driver " DRIVER_NODE);
        return -1;
    }

    ssize_t bytes_written = write(fd, cmd_string, strlen(cmd_string));
    close(fd); // Siempre cerrar, incluso si hay error

    if (bytes_written < 0) {
        perror("liblcd (send_command): Error al escribir al driver");
        return -1;
    }
    return 0; // Éxito
}

/**
 * @brief Función helper INTERNA para leer un string de status del driver.
 */
static int read_from_driver(char* buffer, int buffer_len) {
    // Esta función no cambia. Lee el DRIVER_NODE de lcd_lib.h
    int fd = open(DRIVER_NODE, O_RDONLY);
    if (fd < 0) {
        perror("liblcd (read_from_driver): Error al abrir el driver " DRIVER_NODE);
        return -1;
    }

    // Leer y dejar espacio para el terminador nulo
    ssize_t bytes_read = read(fd, buffer, buffer_len - 1);
    close(fd);

    if (bytes_read < 0) {
        perror("liblcd (read_from_driver): Error al leer del driver");
        return -1;
    }

    buffer[bytes_read] = '\0'; // Asegurar terminación nula
    return 0; // Éxito
}

// --- Implementación de la API Pública ---
// (Todo este código sigue igual, ya que usa los helpers)

int lcd_handshake(void) {
    char response_buffer[32]; // Buffer para la respuesta

    // 1. Enviar el comando de saludo
    if (send_command("HANDSHAKE") != 0) {
        return -1; // Fallo al enviar
    }

    // 2. Leer la respuesta
    if (read_from_driver(response_buffer, sizeof(response_buffer)) != 0) {
        return -1; // Fallo al leer
    }

    // 3. Verificar si la respuesta es la correcta ("ACK_OK")
    if (strcmp(response_buffer, "ACK_OK") == 0) {
        return 0; // ¡Handshake exitoso!
    } else {
        // Error, el driver respondió algo inesperado
        fprintf(stderr, "liblcd (handshake): Handshake fallido. Driver respondió: '%s'\n", response_buffer);
        return -1;
    }
}

int lcd_clear(void) {
    return send_command("CLEAR");
}

int lcd_backlight(int on) {
    char command_buffer[32];
    snprintf(command_buffer, sizeof(command_buffer), "BACKLIGHT %d", (on ? 1 : 0));
    return send_command(command_buffer);
}

int lcd_move_cursor(int row, int col) {
    char command_buffer[32];
    snprintf(command_buffer, sizeof(command_buffer), "MOVE %d %d", row, col);
    return send_command(command_buffer);
}

int lcd_write(const char* text) {
    char command_buffer[256]; // Buffer suficientemente grande
    // Formato: "WRITE <texto...>"
    snprintf(command_buffer, sizeof(command_buffer), "WRITE %s", text);
    return send_command(command_buffer);
}

int lcd_read_status(char* buffer, int buffer_len) {
    // Esta función es simplemente un wrapper para el helper de lectura
    return read_from_driver(buffer, buffer_len);
}