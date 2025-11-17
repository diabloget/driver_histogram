#include "histogram_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>     // Para strncpy, sscanf, strtol, strlen, memset
#include <fcntl.h>      // Para open, O_WRONLY, O_RDONLY
#include <unistd.h>     // Para write, read, close, sleep
#include <sys/ioctl.h>  // Para ioctl
#include <limits.h>     // Para INT_MIN, INT_MAX

// --- Definiciones del Driver (basado en test.c) ---
#define DRIVER_NODE "/dev/lcd_i2c"
#define LCD_CLEAR 0 // El comando IOCTL para limpiar

// --- Definiciones del Histograma ---
#define LCD_BINS 16       // 16 grupos para la pantalla
#define LCD_TECHO 15      // Techo de 15 asteriscos
#define LCD_BAR_WIDTH 15  // 15 caracteres para la barra (1 char para el índice)

// --- Buffer Global para el Histograma Renderizado ---
// Buffer único para el "ticker" deslizable.
// 16 bins * 16 chars/bin = 256.
// Añadimos 32 chars de padding al final (copia del inicio)
// para un scroll "infinito" fácil.
#define HIST_BUFFER_LEN (LCD_BINS * 16)
static char g_histogram_buffer[HIST_BUFFER_LEN + 32];
static int g_buffer_ready = 0; // Flag para saber si el buffer tiene datos


// --- 1. Funciones de Comunicación con el Driver ---

int lcd_write(const char* text) {
    int fd;
    char buffer_32[33]; // 32 chars + 1 terminador nulo

    // 1. Abrir el driver
    fd = open(DRIVER_NODE, O_WRONLY);
    if (fd < 0) {
        perror("libhistogram (lcd_write): Error abriendo driver");
        return -1;
    }

    // 2. Limpiar la pantalla (como en test.c)
    if (ioctl(fd, LCD_CLEAR, 0) < 0) {
        perror("libhistogram (lcd_write): Error enviando comando IOCTL (clear)");
        // No fallamos aquí, solo reportamos.
    }
    
    // 3. Preparar el buffer de 32 caracteres
    strncpy(buffer_32, text, 32);
    buffer_32[32] = '\0'; // Asegurar terminación nula

    // 4. Escribir en el driver
    // Usamos strlen(buffer_32) porque el driver (según test.c)
    // no espera 32 bytes exactos, sino un string.
    ssize_t bytes_written = write(fd, buffer_32, strlen(buffer_32));
    
    close(fd);

    if (bytes_written < 0) {
        perror("libhistogram (lcd_write): Error escribiendo en driver");
        return -1;
    }

    return 0; // Éxito
}

int read_driver(void) {
    int fd;
    char buffer[1025]; // 1024 + 1 nulo
    
    fd = open(DRIVER_NODE, O_RDONLY);
    if (fd < 0) {
        perror("libhistogram (read_driver): No se pudo abrir driver para leer");
        return -1;
    }

    ssize_t bytes_read = read(fd, buffer, 1024);
    close(fd);

    if (bytes_read < 0) {
        perror("libhistogram (read_driver): Error al leer del driver");
        return -1;
    }

    buffer[bytes_read] = '\0'; // Asegurar terminación nula

    printf("--- Inicio de Lectura del Driver ---\n");
    printf("%s", buffer);
    printf("\n--- Fin de Lectura del Driver ---\n");

    return 0;
}

// --- 2. Funciones de Procesamiento de Histograma ---

int generar_histograma(const char* sobel_output, int histogram_out[256]) {
    const char* ptr = sobel_output;
    char* end_ptr;
    long val;
    int min_val = INT_MAX;
    int max_val = INT_MIN;
    int data_points = 0;
    
    // Inicializar histograma
    memset(histogram_out, 0, sizeof(int) * 256);

    // --- Primer Paso: Encontrar Min y Max ---
    // Ignorar la primera línea (ej. "512 512")
    while (*ptr != '\n' && *ptr != '\0') {
        ptr++;
    }
    if (*ptr == '\n') ptr++;

    const char* data_start = ptr; // Guardar inicio de datos

    while (*ptr != '\0') {
        val = strtol(ptr, &end_ptr, 10);
        if (ptr == end_ptr) { // No se leyó número
            ptr++;
            continue;
        }
        if (val > max_val) max_val = val;
        if (val < min_val) min_val = val;
        data_points++;
        ptr = end_ptr; // Avanzar puntero
    }

    if (data_points == 0) return -1; // No se leyeron datos

    // --- Segundo Paso: Llenar Bins ---
    double range = (double)max_val - (double)min_val;
    if (range == 0) range = 1; // Evitar división por cero

    ptr = data_start; // Rebobinar al inicio de los datos
    
    while (*ptr != '\0') {
        val = strtol(ptr, &end_ptr, 10);
        if (ptr == end_ptr) {
            ptr++;
            continue;
        }
        
        // Mapear valor al bin (0-255)
        int bin = (int) ( ((double)val - (double)min_val) / range * 255.0 );
        if (bin < 0) bin = 0;
        if (bin > 255) bin = 255;
        
        histogram_out[bin]++; // Incrementar el contador del bin
        ptr = end_ptr;
    }
    return 0;
}

void comprimir_histograma(const int histogram_full[256], int histogram_grouped_out[LCD_BINS]) {
    int bins_per_group = 256 / LCD_BINS; // 16
    long max_group_val = 0;

    // 1. Agrupar 256 bins en 16 grupos (usando el valor MÁXIMO de cada grupo)
    for (int i = 0; i < LCD_BINS; i++) {
        int group_max = 0;
        for (int j = 0; j < bins_per_group; j++) {
            int bin_index = i * bins_per_group + j;
            if (histogram_full[bin_index] > group_max) {
                group_max = histogram_full[bin_index];
            }
        }
        histogram_grouped_out[i] = group_max;
        
        if (group_max > max_group_val) {
            max_group_val = group_max;
        }
    }

    // 2. Normalizar cada grupo (0-15) basado en el MÁXIMO grupo
    if (max_group_val == 0) { // Histograma vacío
        memset(histogram_grouped_out, 0, sizeof(int) * LCD_BINS);
        return;
    }

    for (int i = 0; i < LCD_BINS; i++) {
        histogram_grouped_out[i] = (int)( ((double)histogram_grouped_out[i] / (double)max_group_val) * (double)LCD_TECHO );
    }
}

/**
 * @brief Función interna para renderizar los 16 grupos
 * en el buffer de string global.
 */
static void render_histogram_buffer(const int hist_grouped[LCD_BINS]) {
    memset(g_histogram_buffer, 0, sizeof(g_histogram_buffer));
    char* ptr = g_histogram_buffer;

    for (int i = 0; i < LCD_BINS; i++) {
        char bar_buffer[LCD_BAR_WIDTH + 1]; // 15 chars + nulo
        int val = hist_grouped[i];

        // Rellenar barra con '*'
        int j = 0;
        for (j = 0; j < val; j++) {
            bar_buffer[j] = '*';
        }
        // Rellenar resto con '-'
        for (; j < LCD_BAR_WIDTH; j++) {
            bar_buffer[j] = '-';
        }
        bar_buffer[LCD_BAR_WIDTH] = '\0'; // Terminar string de barra

        // Escribir al buffer global: "ÍNDICE" (1 char) + "BARRA" (15 chars)
        // Usamos %X para un índice hexadecimal (0-9, A-F)
        int chars_written = sprintf(ptr, "%1X%s", i, bar_buffer);
        ptr += chars_written;
    }
    
    // Copiar los primeros 32 chars al final para un scroll "infinito"
    strncpy(ptr, g_histogram_buffer, 32);
    
    g_buffer_ready = 1; // Marcar buffer como listo
}


void histogram_show(const char* sobel_output) {
    int hist_full[256];
    int hist_grouped[LCD_BINS];

    g_buffer_ready = 0; // Marcar como no listo mientras procesamos

    // 1. Generar
    if (generar_histograma(sobel_output, hist_full) != 0) {
        printf("libhistogram: Error al generar histograma.\n");
        // Llenar buffer con mensaje de error (32 chars)
        lcd_write("ERROR: No se leyo data de Sobel ");
        return;
    }

    // 2. Comprimir
    comprimir_histograma(hist_full, hist_grouped);

    // 3. Renderizar en el buffer global
    render_histogram_buffer(hist_grouped);
}


// --- 3. Funciones de Navegación del Histograma ---

// Función helper interna para `move` y `move_auto`
static int move_internal(int* index, int auto_mode) {
    if (!g_buffer_ready) {
        fprintf(stderr, "libhistogram (move): Error. Debes llamar a histogram_show() primero.\n");
        return -1;
    }
    
    char display_string[33]; // 32 chars + nulo

    // 1. Preparar el string de 32 chars para esta "página"
    // Copia 32 caracteres desde el índice actual del buffer
    strncpy(display_string, &g_histogram_buffer[*index], 32);
    display_string[32] = '\0'; // Asegurar terminación nula

    // 2. Escribir en la LCD
    if (lcd_write(display_string) != 0) {
        fprintf(stderr, "libhistogram (move): Error al escribir en LCD.\n");
        return -1;
    }

    // 3. Avanzar el índice (deslizar la ventana 16 chars)
    *index += 16;
    
    // 4. Hacer wrap-around (volver al inicio)
    // Cuando el índice llega al final del buffer (256), se resetea a 0.
    if (*index >= HIST_BUFFER_LEN) {
        *index = 0;
    }

    // 5. Esperar
    if (auto_mode) {
        sleep(2); // 2 segundos
    } else {
        printf("Mostrando... Presiona ENTER para avanzar (Índice: %d)\n", *index);
        getchar();
    }
    
    return 0;
}

int move(int* index) {
    return move_internal(index, 0); // 0 = modo manual (getchar)
}

int move_auto(int* index) {
    return move_internal(index, 1); // 1 = modo auto (sleep)
}