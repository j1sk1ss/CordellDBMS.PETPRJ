# If machine, where you want to build this database manager,
# don't have make, use command below:
# docker run -it --rm -v $PWD:/work amamory/qemu-stm32 - For STM32 builds
# g++ .\src\main.c .\src\kernel\kentry.c  .\src\kernel\arch\dataman\* .\src\kernel\arch\dirman\* .\src\kernel\arch\pageman\* .\src\kernel\arch\tabman\* .\src\kernel\std\* .\src\userland\std\* -lws2_32 -fpermissive -o cdms_x86-64_win_omp

CC = arm-none-eabi-gcc
CFLAGS = --specs=rdimon.specs -Wl,--start-group -lgcc -lc -lm -lrdimon -Wl,--end-group -s

KERNEL_DIR = src/kernel
KSTD_DIR = $(KERNEL_DIR)/std
ARCH_DIR = $(KERNEL_DIR)/arch
DATABASE_DIR = $(ARCH_DIR)/dataman
DIRMAN_DIR = $(ARCH_DIR)/dirman
PAGEMAN_DIR = $(ARCH_DIR)/pageman
TABMAN_DIR = $(ARCH_DIR)/tabman
MODULE_DIR = $(ARCH_DIR)/module

KSTD_SOURCES = $(wildcard $(KSTD_DIR)/*.c)
DATAMAN_SOURCES = $(wildcard $(DATABASE_DIR)/*.c)
DIRMAN_SOURCES = $(wildcard $(DIRMAN_DIR)/*.c)
PAGEMAN_SOURCES = $(wildcard $(PAGEMAN_DIR)/*.c)
TABMAN_SOURCES = $(wildcard $(TABMAN_DIR)/*.c)
MODULE_SOURCES = $(wildcard $(MODULE_DIR)/*.c)

SOURCES = $(KERNEL_DIR)/kentry.c $(KSTD_SOURCES) $(DATAMAN_SOURCES) $(DIRMAN_SOURCES) $(PAGEMAN_SOURCES) $(TABMAN_SOURCES) $(MODULE_SOURCES)
OUTPUT = builds/cdbms_x86-64

all: force_build $(OUTPUT)
force_build:
	@echo "Force building..."

$(OUTPUT): $(SOURCES)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SOURCES)

clean:
	rm -f $(OUTPUT)

.PHONY: all clean force_build
