CFLAGS = -std=c99 -Wall -Wextra -O2 -ffreestanding -nostdlib -Isrc/libc -Isrc -Isrc/hal -Isrc/core -Isrc/render

CORE_SRCS = src/main.c \
            src/libc/string.c \
            src/libc/stdio.c \
            src/core/buffer.c \
            src/render/render.c

# Windows Target (Default)
WIN_CC = gcc
WIN_TARGET = ectxt.exe
WIN_LDFLAGS = -nostdlib -Wl,-e,mainCRTStartup -lkernel32
WIN_SRCS = $(CORE_SRCS) src/hal/hal_win32.c
WIN_OBJS = $(WIN_SRCS:.c=.win.o)

# Linux x86_64 Target
LINUX_CC ?= x86_64-elf-gcc
LINUX_TARGET = ectxt_linux.elf
LINUX_LDFLAGS = -nostdlib -static -Wl,-e,_start -lgcc
LINUX_SRCS = $(CORE_SRCS) src/hal/hal_linux.c
LINUX_OBJS = $(LINUX_SRCS:.c=.linux.o)

# Linux i686 (32-bit for WebVM / 32-bit kernels) Target
LINUX32_CC ?= i686-elf-gcc
LINUX32_TARGET = ectxt_linux32.elf
LINUX32_LDFLAGS = -nostdlib -static -Wl,-e,_start -lgcc
LINUX32_OBJS = $(LINUX_SRCS:.c=.linux32.o)

all: $(WIN_TARGET)

$(WIN_TARGET): $(WIN_OBJS)
	$(WIN_CC) $(WIN_OBJS) $(WIN_LDFLAGS) -o $@

%.win.o: %.c
	$(WIN_CC) $(CFLAGS) -c $< -o $@

linux: $(LINUX_TARGET)

$(LINUX_TARGET): $(LINUX_OBJS)
	$(LINUX_CC) $(LINUX_OBJS) $(LINUX_LDFLAGS) -o $@

%.linux.o: %.c
	$(LINUX_CC) $(CFLAGS) -c $< -o $@

linux32: $(LINUX32_TARGET)

$(LINUX32_TARGET): $(LINUX32_OBJS)
	$(LINUX32_CC) $(LINUX32_OBJS) $(LINUX32_LDFLAGS) -o $@

%.linux32.o: %.c
	$(LINUX32_CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q /F src\*.o src\libc\*.o src\hal\*.o src\core\*.o src\render\*.o $(WIN_TARGET) $(LINUX_TARGET) $(LINUX32_TARGET) 2>NUL || rm -f $(WIN_OBJS) $(LINUX_OBJS) $(LINUX32_OBJS) $(WIN_TARGET) $(LINUX_TARGET) $(LINUX32_TARGET)

.PHONY: all linux linux32 clean