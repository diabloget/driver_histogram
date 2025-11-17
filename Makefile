# Makefile para crear la biblioteca estática liblcd.a

CC=gcc
CFLAGS=-Wall -c -I.
AR=ar
ARFLAGS=rcs

LIB_NAME=liblcd.a
OBJECTS=lcd_lib.o

# Regla por defecto: construir la biblioteca
all: $(LIB_NAME)

# Regla para crear la biblioteca
$(LIB_NAME): $(OBJECTS)
	@echo "--- Creando Biblioteca $(LIB_NAME) ---"
	$(AR) $(ARFLAGS) $(LIB_NAME) $(OBJECTS)

# Regla para compilar el código C
lcd_lib.o: lcd_lib.c lcd_lib.h
	@echo "Compilando objeto $<..."
	$(CC) $(CFLAGS) $< -o $@

# Limpiar
clean:
	rm -f $(OBJECTS) $(LIB_NAME)