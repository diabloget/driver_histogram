#ifndef LCD_LIB_H
#define LCD_LIB_H

/**
 * @brief El nombre del "archivo" que el driver crea.
 */
#define DRIVER_NODE "/dev/lcd_i2c"

/**
 * @brief Realiza un handshake con el driver para verificar la comunicación.
 * Escribe "HANDSHAKE" y espera recibir "ACK_OK".
 * @return 0 en éxito, -1 en fallo de comunicación.
 */
int lcd_handshake(void);

/**
 * @brief Limpia la pantalla de la LCD.
 * @return 0 en éxito, -1 en error.
 */
int lcd_clear(void);

/**
 * @brief Controla el backlight (luz de fondo) de la LCD.
 * @param on 1 para encender, 0 para apagar.
 * @return 0 en éxito, -1 en error.
 */
int lcd_backlight(int on);

/**
 * @brief Mueve el cursor a una posición específica.
 * @param row Fila (0 o 1).
 * @param col Columna (0-15).
 * @return 0 en éxito, -1 en error.
 */
int lcd_move_cursor(int row, int col);

/**
 * @brief Escribe un string en la LCD en la posición actual del cursor.
 * @param text El texto a mostrar.
 * @return 0 en éxito, -1 en error.
 */
int lcd_write(const char* text);

/**
 * @brief Lee un mensaje de status general desde el driver.
 * @param buffer Puntero a un buffer donde se guardará el status.
 * @param buffer_len Tamaño del buffer.
 * @return 0 en éxito, -1 en error.
 */
int lcd_read_status(char* buffer, int buffer_len);

#endif // LCD_LIB_H