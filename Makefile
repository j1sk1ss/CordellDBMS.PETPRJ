OS := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(OS), Linux)
    include Makefile.linux
else
    include Makefile.windows
endif
