# If machine, where you want to build this database manager,
# don't have make, use command below:
# g++ .\src\main.c .\src\kernel\kentry.c  .\src\kernel\arch\dataman\* .\src\kernel\arch\dirman\* .\src\kernel\arch\pageman\* .\src\kernel\arch\tabman\* .\src\kernel\std\* .\src\userland\std\* -lws2_32 -fpermissive -o cdms_x86-64_win_omp

CC = gcc
CFLAGS = -Wall -Wextra -Ikernel/include -Wcomment -fopenmp -Os -s -DDEBUG -DLOGGING -DWARNINGS -DERRORS -DINFORMING -Wno-unknown-pragmas -Wno-unused-result

USERLAND_DIR = src/userland
KERNEL_DIR = src/kernel
USTD_DIR = $(USERLAND_DIR)/std
KSTD_DIR = $(KERNEL_DIR)/std
ARCH_DIR = $(KERNEL_DIR)/arch
DATABASE_DIR = $(ARCH_DIR)/dataman
DIRMAN_DIR = $(ARCH_DIR)/dirman
PAGEMAN_DIR = $(ARCH_DIR)/pageman
TABMAN_DIR = $(ARCH_DIR)/tabman
MODULE_DIR = $(ARCH_DIR)/module

USTD_SOURCES = $(wildcard $(USTD_DIR)/*.c)
KSTD_SOURCES = $(wildcard $(KSTD_DIR)/*.c)
DATAMAN_SOURCES = $(wildcard $(DATABASE_DIR)/*.c)
DIRMAN_SOURCES = $(wildcard $(DIRMAN_DIR)/*.c)
PAGEMAN_SOURCES = $(wildcard $(PAGEMAN_DIR)/*.c)
TABMAN_SOURCES = $(wildcard $(TABMAN_DIR)/*.c)
MODULE_SOURCES = $(wildcard $(MODULE_DIR)/*.c)

SOURCES = src/main.c $(KERNEL_DIR)/kentry.c $(USTD_SOURCES) $(KSTD_SOURCES) $(DATAMAN_SOURCES) $(DIRMAN_SOURCES) $(PAGEMAN_SOURCES) $(TABMAN_SOURCES) $(MODULE_SOURCES)
OUTPUT = builds/cdbms_x86-64

all: force_build $(OUTPUT)
force_build:
	@echo "Force building..."
	
$(OUTPUT): $(SOURCES)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SOURCES)

clean:
	rm -f $(OUTPUT)

.PHONY: all clean force_build
