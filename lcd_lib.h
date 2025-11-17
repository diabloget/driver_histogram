#ifndef HISTOGRAM_LIB_H
#define HISTOGRAM_LIB_H

#include <stdio.h> // Para size_t

/*
 * histogram_lib.h - API de la biblioteca (Capa 3) para el driver LCD
 * y procesamiento de histogramas (Proyecto II SO).
 */

// --- Constantes de la Biblioteca ---
#define LCD_BINS 16       // 16 grupos para la pantalla
#define LCD_TECHO 15      // Techo de 15 asteriscos
#define LCD_BAR_WIDTH 15  // 15 caracteres para la barra (1 char para el índice)

// --- Funciones de Comunicación con el Driver ---

/**
 * @brief Escribe un string en la pantalla LCD.
 * IMPLEMENTACIÓN: Limpia la pantalla (vía ioctl) y luego
 * escribe los primeros 32 caracteres del string al driver.
 *
 * @param text El string a escribir.
 * @return 0 en éxito, -1 en error.
 */
int lcd_write(const char* text);

/**
 * @brief Lee la salida actual del driver y la imprime en consola.
 * IMPLEMENTACIÓN: Abre el driver en modo lectura (si es posible),
 * lee hasta 1024 bytes y los imprime en stdout.
 *
 * @return 0 en éxito, -1 en error.
 */
int read_driver(void);


// --- Funciones de Procesamiento de Histograma ---

/**
 * @brief Función "Maestra" que procesa la salida Sobel.
 * 1. Llama a 'generar_histograma' (256 bins).
 * 2. Llama a 'comprimir_histograma' (16 grupos, techo 15).
 * 3. Renderiza el resultado en un buffer interno "deslizable".
 *
 * @param sobel_output Un string (char*) que contiene la data
 * completa del archivo output.txt.
 */
void histogram_show(const char* sobel_output);

/**
 * @brief Muestra la siguiente "página" (32 chars) del histograma.
 * Desliza la ventana 16 caracteres sobre el buffer interno.
 * Espera que el usuario presione ENTER para continuar.
 *
 * @param index Puntero al índice actual del buffer (debe ser 0 la
 * primera vez). La función lo actualiza.
 * @return 0 en éxito, -1 en error.
 */
int move(int* index);

/**
 * @brief Muestra la siguiente "página" (32 chars) del histograma.
 * Desliza la ventana 16 caracteres sobre el buffer interno.
 * Espera 2 segundos antes de continuar.
 *
 * @param index Puntero al índice actual del buffer (debe ser 0 la
 * primera vez). La función lo actualiza.
 * @return 0 en éxito, -1 en error.
 */
int move_auto(int* index);


// --- Funciones Internas (Expuestas para Pruebas) ---
// Estas son las funciones que histogram_show usa.

/**
 * @brief Parsea el string de Sobel y genera un histograma de 256 bins.
 *
 * @param sobel_output El string con los datos.
 * @param histogram_out El array (int[256]) donde se guardará el histograma.
 * @return 0 en éxito, -1 si no se encontraron datos.
 */
int generar_histograma(const char* sobel_output, int histogram_out[256]);

/**
 * @brief Comprime el histograma de 256 bins en 16 grupos,
 * con un "techo" de 15.
 *
 * @param histogram_full El histograma de 256 bins.
 * @param histogram_grouped_out El array (int[16]) donde se guardarán
 * los valores normalizados (0-15).
 */
void comprimir_histograma(const int histogram_full[256], int histogram_grouped_out[LCD_BINS]);

#endif // HISTOGRAM_LIB_H