#ifndef HISTOGRAM_LIB_H
#define HISTOGRAM_LIB_H

#include <stdio.h> // Para size_t

/*
 * histogram_lib.h - API de la biblioteca (Capa 3) para el driver LCD
 * y procesamiento de histogramas.
 */

// --- Constantes de la Biblioteca ---
#define LCD_BINS 16       // 16 grupos para la pantalla
#define LCD_TECHO 15      // Techo de 15 asteriscos
#define LCD_BAR_WIDTH 15  // 15 caracteres para la barra (1 char para el índice)

// --- Funciones de Comunicación con el Driver ---

int lcd_write(const char* text);
int read_driver(void);

// --- Funciones de Procesamiento de Histograma ---

void histogram_show(const char* sobel_output);
int  move(int* index);
int  move_auto(int* index);

// Funciones internas (para pruebas)
int  generar_histograma(const char* sobel_output, int histogram_out[256]);
void comprimir_histograma(const int histogram_full[256],
                          int histogram_grouped_out[LCD_BINS]);

#endif // HISTOGRAM_LIB_H
