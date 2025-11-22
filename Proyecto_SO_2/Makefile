###############################################################################
# Proyecto Sobel + MPI (modo software)
# Flujo: make   → compila y ejecuta todo
###############################################################################

CC      = mpicc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
LIBS    = -lpthread -ljpeg -lm

TARGET  = cluster_inicializador

SRCS    = cluster_inicializador.c \
          cluster.c \
          process.c \
          histogram_lib.c \
          metrics.c

OBJS    = $(SRCS:.c=.o)

# --------------------------------------------------------------------------
# OBJETIVO PRINCIPAL: COMPILAR Y EJECUTAR AUTOMÁTICAMENTE
# --------------------------------------------------------------------------
all: $(TARGET)
	@echo "=== Ejecutando clúster automáticamente ==="
	mpirun -np 4 ./$(TARGET)


# --------------------------------------------------------------------------
# COMPILACIÓN DEL EJECUTABLE
# --------------------------------------------------------------------------
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)
	@echo "[OK] Ejecutable generado: ./$(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


# --------------------------------------------------------------------------
# EJECUCIÓN MANUAL (si la necesitás)
# --------------------------------------------------------------------------
run: $(TARGET)
	@echo "=== Ejecutando clúster Sobel ==="
	mpirun -np 4 ./$(TARGET)


# --------------------------------------------------------------------------
# LIMPIEZA
# --------------------------------------------------------------------------
clean:
	rm -f $(OBJS) $(TARGET)
	rm -f ../files/sobel_full.txt
	rm -f ../files/histogram_sobel.csv
	rm -rf output
	@echo "[OK] Limpiado"

.PHONY: all clean run
