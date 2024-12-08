# Enable optimisation flags and use minimalistic libs.
# 1 - Compiler optimisation with only error logs.
# 0 - Full debug with debug flags and debug logs.
PROD ?= 1
# Enable Pthreads (Without it will support only one session at server)
PTHREADS ?= 0
# Enable OpenMP flag
OMP ?= 0
# Enable max optimisation (Disable enviroment vars and exec info (traceback support))
MAX_OPT ?= 1
# Include std libs with -static
INCLUDE_LIBS ?= 0


ifeq ($(PROD), 1)
	CC = musl-gcc
	CFLAGS = -Wall -Wextra -flto -fno-stack-protector -D_FORTUFY_SOURCE=0 -Ikernel/include -Wcomment -Os -s -DWARNINGS -DERRORS -DINFORMING
else
	CC = gcc
	CFLAGS = -Wall -Wextra -Ikernel/include -Wcomment -DDEBUG -DLOGGING -DWARNINGS -DERRORS -DINFORMING -DSPECIAL
endif

ifeq ($(PTHREADS), 0)
	CFLAGS += -DNO_THREADS
endif

ifeq ($(MAX_OPT), 1)
	CFLAGS += -DNO_TRACE -DNO_ENV
endif

ifeq ($(OMP), 1)
	CFLAGS += -fopenmp
endif

ifeq ($(INCLUDE_LIBS), 1)
	CFLAGS += -static
endif


CFLAGS += -Wno-unknown-pragmas -Wno-unused-result -Wno-format-overflow -Wno-empty-body


USERLAND_DIR = src/userland
KERNEL_DIR = src/kernel
USTD_DIR = $(USERLAND_DIR)/std
KSTD_DIR = $(KERNEL_DIR)/std
ARCH_DIR = $(KERNEL_DIR)/arch

SOURCES = src/main.c $(KERNEL_DIR)/kentry.c $(ARCH_DIR)/*/*.c $(KSTD_DIR)/*.c $(USTD_DIR)/*.c
OUTPUT = builds/cdbms_x86-64


all: force_build $(OUTPUT)
force_build:
ifeq ($(shell [ -e $(OUTPUT) ] && echo yes), yes)
	@rm -f $(OUTPUT)
endif
	
$(OUTPUT): $(SOURCES)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SOURCES)

clean:
	rm -f $(OUTPUT)

.PHONY: all clean force_build
