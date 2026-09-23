ifeq ($(OS),Windows_NT)
    HOST_OS := Windows
    CC := gcc
    TARGET := ectxt.exe
    CLEAN_CMD = del /Q /F src\*.o src\libc\*.o src\hal\*.o src\core\*.o $(TARGET) 2>NUL || exit 0
else
    HOST_OS := $(shell uname -s)
    CC := gcc
    TARGET := ectxt
    CLEAN_CMD = rm -f src/*.o src/libc/*.o src/hal/*.o src/core/*.o $(TARGET)
endif

CFLAGS = -std=c99 -Wall -Wextra -O2 -ffreestanding -nostdlib -Isrc/libc -Isrc -Isrc/hal -Isrc/core

ifeq ($(HOST_OS),Windows)
    LDFLAGS = -nostdlib -Wl,-e,mainCRTStartup -lkernel32
    HAL_SRC = src/hal/hal_win32.c
else
    LDFLAGS = -nostdlib -static
    HAL_SRC = src/hal/hal_posix.c
endif

SRCS = src/main.c \
       src/libc/string.c \
       src/libc/stdio.c \
       src/core/buffer.c \
       $(HAL_SRC)

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@$(CLEAN_CMD)

.PHONY: all clean