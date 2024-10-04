CC = gcc-14
CFLAGS = -Wall -Wextra -Ikernel/include -fopenmp

KERNEL_DIR = src/kernel
STD_DIR = $(KERNEL_DIR)/std
ARCH_DIR = $(KERNEL_DIR)/arch
DATABASE_C = $(ARCH_DIR)/dataman.c
DIRMAN_DIR = $(ARCH_DIR)/dirman
PAGEMAN_DIR = $(ARCH_DIR)/pageman
TABMAN_DIR = $(ARCH_DIR)/tabman

STD_SOURCES = $(wildcard $(STD_DIR)/*.c)
DIRMAN_SOURCES = $(wildcard $(DIRMAN_DIR)/*.c)
PAGEMAN_SOURCES = $(wildcard $(PAGEMAN_DIR)/*.c)
TABMAN_SOURCES = $(wildcard $(TABMAN_DIR)/*.c)

SOURCES = src/main.c $(KERNEL_DIR)/kentry.c $(STD_SOURCES) $(DATABASE_C) $(DIRMAN_SOURCES) $(PAGEMAN_SOURCES) $(TABMAN_SOURCES)

OUTPUT = builds/cdbms_x86-64

all: $(OUTPUT)

$(OUTPUT): $(SOURCES)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SOURCES)

clean:
	rm -f $(OUTPUT)

.PHONY: all clean
