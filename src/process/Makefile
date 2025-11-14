# Makefile para el proyecto driver_histogram
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -g
PTHREAD = -pthread
LIBS = -ljpeg $(PTHREAD)
SRC := $(wildcard *.c)
OBJDIR := obj
OBJ := $(patsubst %.c,$(OBJDIR)/%.o,$(SRC))
DEPS := $(wildcard *.h)
TARGET := $(OBJDIR)/test 

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/%.o: %.c $(DEPS)
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) $(PTHREAD) -c $< -o $@

test: $(TARGET)
	@echo "Ejecutando $(TARGET) (timeout 5s)..."
	timeout 5s ./$(TARGET) || true

clean:
	rm -rf $(OBJDIR)


