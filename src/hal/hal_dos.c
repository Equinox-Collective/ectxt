#include "hal.h"
#include "../libc/string.h"

extern int ectxt_main(void);
extern char __bss_start[];
extern char __bss_end[];

static uint16_t vga_backbuffer[80 * 25];
static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t current_attr = 0x07;
static char cli_file_arg[128];

static char static_file_pool[16384];
static char alloc_pool[24576];
static size_t alloc_offset = 0;

static void vga_set_hw_cursor(int x, int y) {
    if (x < 0) x = 0;
    if (x >= 80) x = 79;
    if (y < 0) y = 0;
    if (y >= 25) y = 24;

    uint16_t dx_val = ((uint16_t)(y & 0xFF) << 8) | (uint16_t)(x & 0xFF);
    __asm__ volatile (
        ".code16\n"
        "movw $0x0200, %%ax\n"
        "xorw %%bx, %%bx\n"
        "int $0x10\n"
        :
        : "d"(dx_val)
        : "eax", "ebx"
    );
}

static void vga_flush(void) {
    __asm__ volatile (
        ".code16\n"
        "pushw %%es\n"
        "movw $0xb800, %%ax\n"
        "movw %%ax, %%es\n"
        "xorw %%di, %%di\n"
        "movw $2000, %%cx\n"
        "cld\n"
        "rep movsw\n"
        "popw %%es\n"
        :
        : "S"((uint16_t)(uintptr_t)vga_backbuffer)
        : "eax", "ecx", "edi", "memory"
    );
    vga_set_hw_cursor(cursor_x, cursor_y);
}

int hal_init(void) {
    cursor_x = 0;
    cursor_y = 0;
    current_attr = 0x07;
    for (int i = 0; i < 80 * 25; i++) {
        vga_backbuffer[i] = (uint16_t)(' ' | (current_attr << 8));
    }
    vga_flush();
    return 1;
}

void hal_shutdown(void) {
    cursor_x = 0;
    cursor_y = 0;
    current_attr = 0x07;
    for (int i = 0; i < 80 * 25; i++) {
        vga_backbuffer[i] = (uint16_t)(' ' | (0x07 << 8));
    }
    vga_flush();
}

void hal_get_term_size(int *cols, int *rows) {
    *cols = 80;
    *rows = 25;
}

void hal_write(const void *buf, size_t len) {
    const uint8_t *b = (const uint8_t *)buf;
    for (size_t i = 0; i < len; i++) {
        if (b[i] == 27 && i + 1 < len && b[i + 1] == '[') {
            size_t j = i + 2;
            int num1 = 0, num2 = 0;
            int has_num1 = 0;

            if (b[j] == '?') {
                while (j < len && b[j] != 'h' && b[j] != 'l') j++;
                if (j < len && b[j] == 'h') vga_flush();
                i = j;
                continue;
            }

            while (j < len && b[j] >= '0' && b[j] <= '9') {
                num1 = num1 * 10 + (b[j] - '0');
                has_num1 = 1;
                j++;
            }

            if (j < len && b[j] == ';') {
                j++;
                while (j < len && b[j] >= '0' && b[j] <= '9') {
                    num2 = num2 * 10 + (b[j] - '0');
                    j++;
                }
            }

            if (j < len) {
                if (b[j] == 'H') {
                    cursor_y = has_num1 ? num1 - 1 : 0;
                    cursor_x = num2 > 0 ? num2 - 1 : 0;
                    if (cursor_y < 0) cursor_y = 0;
                    if (cursor_y >= 25) cursor_y = 24;
                    if (cursor_x < 0) cursor_x = 0;
                    if (cursor_x >= 80) cursor_x = 79;
                } else if (b[j] == 'K') {
                    if (cursor_y >= 0 && cursor_y < 25) {
                        int start_c = cursor_x >= 0 ? cursor_x : 0;
                        for (int c = start_c; c < 80; c++) {
                            vga_backbuffer[cursor_y * 80 + c] = (uint16_t)(' ' | (current_attr << 8));
                        }
                    }
                } else if (b[j] == 'm') {
                    if (num1 == 7) current_attr = 0x70;
                    else if (num1 == 0) current_attr = 0x07;
                }
                i = j;
                continue;
            }
        }

        if (b[i] == '\r') {
            cursor_x = 0;
        } else if (b[i] == '\n') {
            cursor_y++;
            if (cursor_y >= 25) cursor_y = 24;
        } else {
            if (cursor_x >= 0 && cursor_x < 80 && cursor_y >= 0 && cursor_y < 25) {
                vga_backbuffer[cursor_y * 80 + cursor_x] = (uint16_t)(b[i] | (current_attr << 8));
                cursor_x++;
            }
        }
    }
    vga_flush();
}

int hal_read_key(void) {
    uint16_t key_val = 0;
    __asm__ volatile (
        ".code16\n"
        "xorw %%ax, %%ax\n"
        "int $0x16\n"
        : "=a"(key_val)
        :
        : "memory"
    );

    uint8_t ascii = (uint8_t)(key_val & 0xFF);
    uint8_t scancode = (uint8_t)(key_val >> 8);

    if (ascii != 0 && ascii != 0xE0) {
        if (ascii == 8) return KEY_BACKSPACE;
        if (ascii == 13) return KEY_ENTER;
        if (ascii == 27) return KEY_ESCAPE;
        return (int)ascii;
    }

    switch (scancode) {
        case 0x48: return KEY_ARROW_UP;
        case 0x50: return KEY_ARROW_DOWN;
        case 0x4B: return KEY_ARROW_LEFT;
        case 0x4D: return KEY_ARROW_RIGHT;
        case 0x47: return KEY_HOME;
        case 0x4F: return KEY_END;
        case 0x49: return KEY_PAGE_UP;
        case 0x51: return KEY_PAGE_DOWN;
        case 0x53: return KEY_DELETE;
    }

    return KEY_NONE;
}


void *hal_alloc(size_t size) {
    size = (size + 3) & ~3;
    if (alloc_offset + size > sizeof(alloc_pool)) {
        return (void *)0;
    }
    void *ptr = &alloc_pool[alloc_offset];
    alloc_offset += size;
    return ptr;
}

void hal_free(void *ptr, size_t size) {
    (void)ptr;
    (void)size;
}

int hal_file_read(const char *path, char **out_buf, size_t *out_size) {
    uint16_t handle = 0;
    uint16_t err = 0;

    __asm__ volatile (
        ".code16\n"
        "movw $0x3d00, %%ax\n"
        "int $0x21\n"
        "jnc 1f\n"
        "movw $1, %1\n"
        "1:\n"
        : "=a"(handle), "=r"(err)
        : "d"((uint16_t)(uintptr_t)path), "1"(0)
        : "memory"
    );

    if (err) return 0;

    uint16_t bytes_read = 0;
    uint16_t max_read = (uint16_t)(sizeof(static_file_pool) - 1);

    __asm__ volatile (
        ".code16\n"
        "movb $0x3f, %%ah\n"
        "int $0x21\n"
        : "=a"(bytes_read)
        : "b"(handle), "c"(max_read), "d"((uint16_t)(uintptr_t)static_file_pool)
        : "memory"
    );

    __asm__ volatile (
        ".code16\n"
        "movb $0x3e, %%ah\n"
        "int $0x21\n"
        :
        : "b"(handle)
        : "eax"
    );

    static_file_pool[bytes_read] = '\0';
    *out_buf = static_file_pool;
    *out_size = bytes_read;
    return 1;
}

int hal_file_write_chunks(const char *path, const void *p1, size_t n1, const void *p2, size_t n2) {
    uint16_t handle = 0;
    uint16_t err = 0;

    __asm__ volatile (
        ".code16\n"
        "movb $0x3c, %%ah\n"
        "xorw %%cx, %%cx\n"
        "int $0x21\n"
        "jnc 1f\n"
        "movw $1, %1\n"
        "1:\n"
        : "=a"(handle), "=r"(err)
        : "d"((uint16_t)(uintptr_t)path), "1"(0)
        : "ecx", "memory"
    );

    if (err) return 0;

    if (n1 > 0) {
        __asm__ volatile (
            ".code16\n"
            "movb $0x40, %%ah\n"
            "int $0x21\n"
            :
            : "b"(handle), "c"((uint16_t)n1), "d"((uint16_t)(uintptr_t)p1)
            : "eax", "memory"
        );
    }

    if (n2 > 0) {
        __asm__ volatile (
            ".code16\n"
            "movb $0x40, %%ah\n"
            "int $0x21\n"
            :
            : "b"(handle), "c"((uint16_t)n2), "d"((uint16_t)(uintptr_t)p2)
            : "eax", "memory"
        );
    }

    __asm__ volatile (
        ".code16\n"
        "movb $0x3e, %%ah\n"
        "int $0x21\n"
        :
        : "b"(handle)
        : "eax"
    );

    return 1;
}

const char *hal_get_cli_argument(void) {
    uint8_t len = *(const uint8_t *)0x80;
    if (len == 0) return (void *)0;

    const char *src = (const char *)0x81;
    while (len > 0 && *src == ' ') {
        src++;
        len--;
    }

    size_t i = 0;
    while (len > 0 && *src != '\r' && *src != ' ' && i < sizeof(cli_file_arg) - 1) {
        cli_file_arg[i++] = *src++;
        len--;
    }
    cli_file_arg[i] = '\0';
    return i > 0 ? cli_file_arg : (void *)0;
}

void dos_entry(void) {
    ectxt_main();
}

__asm__ (
    ".code16\n"
    ".section .entry, \"ax\"\n"
    ".global _start\n"
    "_start:\n"
    "cld\n"
    "xor %al, %al\n"
    "mov $__bss_start, %di\n"
    "mov $__bss_end, %cx\n"
    "sub %di, %cx\n"
    "rep stosb\n"
    "call dos_entry\n"
    "movb $0x4c, %ah\n"
    "xorb %al, %al\n"
    "int $0x21\n"
);