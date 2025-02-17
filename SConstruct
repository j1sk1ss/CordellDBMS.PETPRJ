import os
from SCons.Script import Environment, ARGUMENTS


PROD = int(ARGUMENTS.get('PROD', 0))
PTHREADS = int(ARGUMENTS.get('PTHREADS', 0))
OMP = int(ARGUMENTS.get('OMP', 0))
MAX_OPT = int(ARGUMENTS.get('MAX_OPT', 1))
INCLUDE_LIBS = int(ARGUMENTS.get('INCLUDE_LIBS', 0))

DISABLE_UPDATE = int(ARGUMENTS.get('DISABLE_UPDATE', 0))
DISABLE_GET_EXPRESSION = int(ARGUMENTS.get('DISABLE_GET_EXPRESSION', 0))
DISABLE_GET_VALUE = int(ARGUMENTS.get('DISABLE_GET_VALUE', 0))
DISABLE_DELETE = int(ARGUMENTS.get('DISABLE_DELETE', 0))
DISABLE_CREATE = int(ARGUMENTS.get('DISABLE_CREATE', 0))
DISABLE_MIGRATION = int(ARGUMENTS.get('DISABLE_MIGRATION', 0))


if PROD:
    CC = "musl-gcc"
    CFLAGS = ["-Wall", "-Wextra", "-flto", "-fno-stack-protector", "-D_FORTUFY_SOURCE=0", "-Ikernel/include", "-Wcomment", "-Os", "-s", "-DWARNINGS", "-DERRORS", "-DINFORMING"]
else:
    CC = "C:\\msys64\\ucrt64\\bin\\gcc.exe"
    CFLAGS = ["-v", "-Wall", "-Wextra", "-Ikernel/include", "-Wcomment", "-DDEBUG", "-DLOGGING", "-DWARNINGS", "-DERRORS", "-DINFORMING", "-DSPECIAL"]


if DISABLE_UPDATE:
    CFLAGS.append("-DNO_UPDATE_COMMAND")
if DISABLE_GET_EXPRESSION:
    CFLAGS.append("-DNO_GET_EXPRESSION_COMMAND")
if DISABLE_GET_VALUE:
    CFLAGS.append("-DNO_GET_VALUE_COMMAND")
if DISABLE_DELETE:
    CFLAGS.append("-DNO_DELETE_COMMAND")
if DISABLE_CREATE:
    CFLAGS.append("-DNO_CREATE_COMMAND")
if DISABLE_MIGRATION:
    CFLAGS.append("-DNO_MIGRATE_COMMAND")
if not PTHREADS:
    CFLAGS.append("-DNO_THREADS")
if MAX_OPT:
    CFLAGS.append("-DNO_TRACE")
    CFLAGS.append("-DNO_ENV")
if OMP:
    CFLAGS.append("-fopenmp")
if INCLUDE_LIBS:
    CFLAGS.append("-static")


CFLAGS.extend(["-Wno-unknown-pragmas", "-Wno-unused-result", "-Wno-format-overflow", "-Wno-empty-body", "-Wno-unused-parameter"])


USERLAND_DIR = "src/userland"
KERNEL_DIR = "src/kernel"
USTD_DIR = os.path.join(USERLAND_DIR, "std")
KSTD_DIR = os.path.join(KERNEL_DIR, "std")
ARCH_DIR = os.path.join(KERNEL_DIR, "arch")

sources = ["src/main.c", os.path.join(KERNEL_DIR, "kentry.c")]
sources.extend([os.path.join(ARCH_DIR, "*/", "*.c"), os.path.join(KSTD_DIR, "*.c"), os.path.join(USTD_DIR, "*.c")])

output = "builds/cdbms_x86-64"

env = Environment(CC=CC, CFLAGS=CFLAGS)
env.Program(target=output, source=sources)

Clean(".", output)