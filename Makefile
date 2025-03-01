# Enable optimisation flags and use minimalistic libs.
# 1 - Compiler optimisation with only error logs.
# 0 - Full debug with debug flags and debug logs.
PROD ?= 0
# Enable Pthreads (Without it will support only one session at server)
# 1 - Pthreads enabled.
PTHREADS ?= 0
# Enable OpenMP flag
# 1 - OpenMP enabled.
OMP ?= 0
# Enable max optimisation (Disable enviroment vars and exec info (traceback support))
MAX_OPT ?= 0
# Include std libs with -static
INCLUDE_LIBS ?= 0
# User check at start. Will give max access to any session.
# 1 - User (username and pass required) enabled.
USERS ?= 1
# Server type of compilation
# Disable server side code. Instead server, kernel take char* command.
SERVER ?= 1 # NO_SERVER
# Enable profiler
DEBUG_PROFILER ?= 0

# Kernel commands setup.
# Log to .log file
FILE_LOGGING ?= 0
# Disable all commands with update functionality
# Exmpl: insert / update by_index / update by_value
DISABLE_UPDATE ?= 0
# Disable get by_exp command
DISABLE_GET_EXPRESSION ?= 0
# Disable get by_value command
DISABLE_GET_VALUE ?= 0
# Desable all commands with delete functionality
# Exmpl: delete table / delete row / delete database
DISABLE_DELETE ?= 0
# Disable all commands with create functionality
# Exmpl: create table / create database
DISABLE_CREATE ?= 0
# Disable all commands with migration functionality
# Exmpl: migrate table
DISABLE_MIGRATION ?= 0
# Disable check signature in append and insert data
DISABLE_CHECK_SIGNATURE ?= 0

# Logger flags
ERROR_LOGS ?= 1
WARN_LOGS ?= 0
INFO_LOGS ?= 1
DEBUG_LOGS ?= 1
IO_LOGS ?= 0
MEM_LOGS ?= 0
LOGGING_LOGS ?= 1
SPECIAL_LOGS ?= 1

#########
# Base flags
CFLAGS = -Wall -Wextra -Ikernel/include -Wcomment -Wno-unknown-pragmas -Wno-unused-result -Wno-format-overflow -Wno-empty-body -Wno-unused-parameter
ifeq ($(PROD), 1)
    CC = musl-gcc
    CFLAGS += -Os -s -flto -fno-stack-protector -D_FORTIFY_SOURCE=0
else
    CC = gcc-14
endif

########
# Logger flags
ifeq ($(ERROR_LOGS), 1)
    CFLAGS += -DERROR_LOGS
endif

ifeq ($(WARN_LOGS), 1)
    CFLAGS += -DWARNING_LOGS
endif

ifeq ($(INFO_LOGS), 1)
    CFLAGS += -DINFO_LOGS
endif

ifeq ($(DEBUG_LOGS), 1)
    CFLAGS += -DDEBUG_LOGS
endif

ifeq ($(IO_LOGS), 1)
    CFLAGS += -DIO_OPERATION_LOGS
endif

ifeq ($(MEM_LOGS), 1)
    CFLAGS += -DMEM_OPERATION_LOGS
endif

ifeq ($(LOGGING_LOGS), 1)
    CFLAGS += -DLOGGING_LOGS
endif

ifeq ($(SPECIAL_LOGS), 1)
    CFLAGS += -DSPECIAL_LOGS
endif

########
# Settings flags
ifeq ($(DEBUG_PROFILER), 1)
    CFLAGS += -pg
endif

ifeq ($(FILE_LOGGING), 1)
    CFLAGS += -DLOG_TO_FILE
endif

ifeq ($(DISABLE_UPDATE), 1)
    CFLAGS += -DNO_UPDATE_COMMAND
endif

ifeq ($(DISABLE_CHECK_SIGNATURE), 1)
    CFLAGS += -DDISABLE_CHECK_SIGNATURE
endif

ifeq ($(DISABLE_GET_EXPRESSION), 1)
    CFLAGS += -DNO_GET_EXPRESSION_COMMAND
endif

ifeq ($(DISABLE_GET_VALUE), 1)
    CFLAGS += -DNO_GET_VALUE_COMMAND
endif

ifeq ($(DISABLE_DELETE), 1)
    CFLAGS += -DNO_DELETE_COMMAND
endif

ifeq ($(DISABLE_CREATE), 1)
    CFLAGS += -DNO_CREATE_COMMAND
endif

ifeq ($(DISABLE_MIGRATION), 1)
    CFLAGS += -DNO_MIGRATE_COMMAND
endif

ifeq ($(USERS), 0)
    CFLAGS += -DNO_USER
endif

ifeq ($(SERVER), 0)
    CFLAGS += -DNO_SERVER
endif

ifeq ($(PTHREADS), 0)
    CFLAGS += -DNO_THREADS
endif

ifeq ($(MAX_OPT), 1)
    CFLAGS += -DNO_TRACE -DNO_ENV -DNO_VERSION_COMMAND
endif

ifeq ($(OMP), 1)
    CFLAGS += -fopenmp
endif

ifeq ($(INCLUDE_LIBS), 1)
    CFLAGS += -static
endif

#############################
# Build part
OS := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(OS),Linux)
    UNIX_BASED := 1
else ifeq ($(OS),Darwin)
    UNIX_BASED := 1
endif

ifeq ($(UNIX_BASED),1)
    OUTPUT = builds/cdbms_x86-64
    KERNEL_DIR = src/kernel
    KSTD_DIR = $(KERNEL_DIR)/std
    ARCH_DIR = $(KERNEL_DIR)/arch
    SOURCES = src/main.c $(KERNEL_DIR)/kentry.c $(ARCH_DIR)/*/*.c $(KSTD_DIR)/*.c

    all: force_build $(OUTPUT)
    
    force_build:
	@if [ -e $(OUTPUT) ]; then rm -f $(OUTPUT); fi

else
    CFLAGS += -D__USE_MINGW_ANSI_STDIO
    OUTPUT = builds/cdbms_x86-64.exe
    KERNEL_DIR = src/kernel
    KSTD_DIR = $(KERNEL_DIR)/std
    ARCH_DIR = $(KERNEL_DIR)/arch
    ARCH_SOURCES := $(shell dir /s /b src\kernel\arch\*.c)
    KSTD_SOURCES := $(shell dir /s /b src\kernel\std\*.c)
    SOURCES = src/main.c $(KERNEL_DIR)/kentry.c $(ARCH_SOURCES) $(KSTD_SOURCES) -lws2_32

    all: $(OUTPUT)
endif

	
$(OUTPUT): $(SOURCES)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SOURCES)

clean:
	rm -f $(OUTPUT)

.PHONY: all clean force_build