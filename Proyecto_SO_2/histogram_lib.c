#include "histogram_lib.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>     // open
#include <unistd.h>    // write, read, close, sleep
#include <sys/ioctl.h> // ioctl
#include <limits.h>    // INT_MIN, INT_MAX
#include <errno.h>

// --- Definiciones del Driver (basado en test.c) ---
#define DRIVER_NODE "/dev/lcd_i2c"
#define LCD_CLEAR 0 // El comando IOCTL para limpiar

// --- Buffer Global para el Histograma Renderizado ---
#define HIST_BUFFER_LEN (LCD_BINS * 16)

static char g_histogram_buffer[HIST_BUFFER_LEN + 32];
static int g_buffer_ready = 0; // Flag para saber si el buffer tiene datos

// ======================================================================
// 1. Funciones de Comunicación con el Driver
// ======================================================================

int lcd_write(const char *text)
{
    int fd;
    char buffer_32[33]; // 32 chars + 1 terminador nulo

    fd = open(DRIVER_NODE, O_WRONLY);
    if (fd < 0)
    {
        perror("libhistogram (lcd_write): Error abriendo driver");
        return -1;
    }

    if (ioctl(fd, LCD_CLEAR, 0) < 0)
    {
        perror("libhistogram (lcd_write): Error enviando comando IOCTL (clear)");
    }

    snprintf(buffer_32, sizeof(buffer_32), "%s", text);

    ssize_t bytes_written = write(fd, buffer_32, strlen(buffer_32));

    close(fd);

    if (bytes_written < 0)
    {
        perror("libhistogram (lcd_write): Error escribiendo en driver");
        return -1;
    }

    return 0;
}

int read_driver(void)
{
    int fd;
    char buffer[1025]; // 1024 + 1 nulo

    fd = open(DRIVER_NODE, O_RDONLY);
    if (fd < 0)
    {
        perror("libhistogram (read_driver): No se pudo abrir driver para leer");
        return -1;
    }

    ssize_t bytes_read = read(fd, buffer, 1024);
    close(fd);

    if (bytes_read < 0)
    {
        perror("libhistogram (read_driver): Error al leer del driver");
        return -1;
    }

    buffer[bytes_read] = '\0';

    printf("--- Inicio de Lectura del Driver ---\n");
    printf("%s", buffer);
    printf("\n--- Fin de Lectura del Driver ---\n");

    return 0;
}

// ======================================================================
// 2. Funciones de Procesamiento de Histograma
// ======================================================================

int generar_histograma(const char *sobel_output, int histogram_out[256])
{
    const char *ptr = sobel_output;
    char *end_ptr;
    long val;
    int min_val = INT_MAX;
    int max_val = INT_MIN;
    int data_points = 0;

    memset(histogram_out, 0, sizeof(int) * 256);

    // Ignorar la primera línea ("h w")
    while (*ptr != '\n' && *ptr != '\0')
    {
        ptr++;
    }
    if (*ptr == '\n')
        ptr++;

    const char *data_start = ptr;

    while (*ptr != '\0')
    {
        val = strtol(ptr, &end_ptr, 10);
        if (ptr == end_ptr)
        {
            ptr++;
            continue;
        }
        if (val > max_val)
            max_val = val;
        if (val < min_val)
            min_val = val;
        data_points++;
        ptr = end_ptr;
    }

    if (data_points == 0)
        return -1;

    double range = (double)max_val - (double)min_val;
    if (range == 0)
        range = 1;

    ptr = data_start;

    while (*ptr != '\0')
    {
        val = strtol(ptr, &end_ptr, 10);
        if (ptr == end_ptr)
        {
            ptr++;
            continue;
        }
        int bin = (int)(((double)val - (double)min_val) / range * 255.0);
        if (bin < 0)
            bin = 0;
        if (bin > 255)
            bin = 255;

        histogram_out[bin]++;
        ptr = end_ptr;
    }
    return 0;
}

void comprimir_histograma(const int histogram_full[256],
                          int histogram_grouped_out[LCD_BINS])
{
    int bins_per_group = 256 / LCD_BINS; // 16
    long max_group_val = 0;

    for (int i = 0; i < LCD_BINS; i++)
    {
        int group_max = 0;
        for (int j = 0; j < bins_per_group; j++)
        {
            int bin_index = i * bins_per_group + j;
            if (histogram_full[bin_index] > group_max)
            {
                group_max = histogram_full[bin_index];
            }
        }
        histogram_grouped_out[i] = group_max;

        if (group_max > max_group_val)
        {
            max_group_val = group_max;
        }
    }

    if (max_group_val == 0)
    {
        memset(histogram_grouped_out, 0, sizeof(int) * LCD_BINS);
        return;
    }

    for (int i = 0; i < LCD_BINS; i++)
    {
        histogram_grouped_out[i] =
            (int)(((double)histogram_grouped_out[i] /
                   (double)max_group_val) *
                  (double)LCD_TECHO);
    }
}

static void render_histogram_buffer(const int hist_grouped[LCD_BINS])
{
    memset(g_histogram_buffer, 0, sizeof(g_histogram_buffer));
    char *ptr = g_histogram_buffer;

    for (int i = 0; i < LCD_BINS; i++)
    {
        char bar_buffer[LCD_BAR_WIDTH + 1];
        int val = hist_grouped[i];

        int j = 0;
        for (j = 0; j < val; j++)
        {
            bar_buffer[j] = '*';
        }
        for (; j < LCD_BAR_WIDTH; j++)
        {
            bar_buffer[j] = '-';
        }
        bar_buffer[LCD_BAR_WIDTH] = '\0';

        int chars_written = sprintf(ptr, "%1X%s", i, bar_buffer);
        ptr += chars_written;
    }

    memcpy(ptr, g_histogram_buffer, 32);

    g_buffer_ready = 1;
}

void histogram_show(const char *sobel_output)
{
    int hist_full[256];
    int hist_grouped[LCD_BINS];

    g_buffer_ready = 0;

    if (generar_histograma(sobel_output, hist_full) != 0)
    {
        printf("libhistogram: Error al generar histograma.\n");
        lcd_write("ERROR: No se leyo data de Sobel ");
        return;
    }

    comprimir_histograma(hist_full, hist_grouped);
    render_histogram_buffer(hist_grouped);
}

// ======================================================================
// 3. Navegación
// ======================================================================

static int move_internal(int *index, int auto_mode)
{
    if (!g_buffer_ready)
    {
        fprintf(stderr,
                "libhistogram (move): Error. Debes llamar a histogram_show() primero.\n");
        return -1;
    }

    char display_string[33];

    memcpy(display_string, &g_histogram_buffer[*index], 32);
    display_string[32] = '\0';

    if (lcd_write(display_string) != 0)
    {
        fprintf(stderr, "libhistogram (move): Error al escribir en LCD.\n");
        return -1;
    }

    *index += 16;

    if (*index >= HIST_BUFFER_LEN)
    {
        *index = 0;
    }

    if (auto_mode)
    {
        sleep(2);
    }
    else
    {
        printf("Mostrando... Presiona ENTER para avanzar (Índice: %d)\n", *index);
        getchar();
    }

    return 0;
}

int move(int *index)
{
    return move_internal(index, 0);
}

int move_auto(int *index)
{
    return move_internal(index, 1);
}
