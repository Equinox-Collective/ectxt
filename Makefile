CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -O2 -ffreestanding -nostdlib -Isrc/libc -Isrc
LDFLAGS ?= -Wl,-e,mainCRTStartup -lkernel32

TARGET = ectxt.exe

SRCS = src/main.c \
       src/libc/string.c \
       src/libc/stdio.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q /F src\*.o src\libc\*.o $(TARGET) 2>NUL || rm -f $(OBJS) $(TARGET)

.PHONY: all clean