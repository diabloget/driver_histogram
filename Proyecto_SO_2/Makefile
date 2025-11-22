# Makefile para compilar y probar el proyecto driver_histogram

# Compilador y banderas
CC = mpicc
CFLAGS = -Wall -Wextra -O2 -lm
TARGET_CLUSTER = cluster_test
TARGET_INIT = cluster_inicializador
SRC = cluster.c cluster_inicializador.c process.c histogram_lib.c metrics.c
TEST_SRC = pruebas/test_cluster_flow.c
INCLUDES = -I.

# Archivos generados
OUTPUT_DIR = output
FILES_DIR = ../files
SOBEL_OUTPUT = $(FILES_DIR)/sobel_full.txt
HISTOGRAM_OUTPUT = $(FILES_DIR)/histogram_sobel.csv

# Reglas principales
all: $(TARGET_CLUSTER) $(TARGET_INIT)

# Compilar el ejecutable principal del clúster
$(TARGET_CLUSTER): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

# Compilar el inicializador del clúster
$(TARGET_INIT): cluster_inicializador.c cluster.c process.c histogram_lib.c metrics.c
	$(CC) $(CFLAGS) -o $@ $^

# Compilar y ejecutar las pruebas
test: $(TARGET_CLUSTER)
	@echo "=== Ejecutando pruebas ==="
	$(CC) $(CFLAGS) -o test_cluster_flow $(TEST_SRC) cluster.c process.c histogram_lib.c metrics.c
	mpirun -np 4 ./test_cluster_flow

# Ejecutar el flujo completo del clúster
run: $(TARGET_INIT)
	@echo "=== Ejecutando flujo completo del clúster ==="
	mpirun -np 4 ./$(TARGET_INIT)

# Validar resultados con el script de Python
validate:
	@echo "=== Validando resultados ==="
	python3 pruebas/validar_resultados.py

# Limpiar archivos generados
clean:
	rm -f $(TARGET_CLUSTER) $(TARGET_INIT) test_cluster_flow
	rm -rf $(OUTPUT_DIR)
	rm -f $(SOBEL_OUTPUT) $(HISTOGRAM_OUTPUT)

# Ayuda
help:
	@echo "Comandos disponibles:"
	@echo "  make              - Compila los binarios principales"
	@echo "  make test         - Compila y ejecuta las pruebas"
	@echo "  make run          - Ejecuta el flujo completo del clúster"
	@echo "  make validate     - Valida los resultados con el script de Python"
	@echo "  make clean        - Limpia los archivos generados"