#include "hal.h"
#include "../libc/string.h"

#define TCGETS      0x5401
#define TCSETS      0x5402
#define TIOCGWINSZ  0x5413

#define O_RDONLY    0
#define O_WRONLY    1
#define O_CREAT     0x40
#define O_TRUNC     0x200

#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define MAP_PRIVATE 0x02
#define MAP_ANON    0x20

#define NCCS 19

struct linux_termios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t  c_line;
    uint8_t  c_cc[NCCS];
};

struct linux_winsize {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
};

static struct linux_termios orig_termios;
static const char *cli_filename = (void *)0;

extern int ectxt_main(void);

#if defined(__x86_64__)

#define SYS_READ    0
#define SYS_WRITE   1
#define SYS_OPEN    2
#define SYS_CLOSE   3
#define SYS_MMAP    9
#define SYS_MUNMAP  11
#define SYS_IOCTL   16
#define SYS_EXIT    60

static inline intptr_t sys_call1(intptr_t num, intptr_t a1) {
    intptr_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static inline intptr_t sys_call2(intptr_t num, intptr_t a1, intptr_t a2) {
    intptr_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static inline intptr_t sys_call3(intptr_t num, intptr_t a1, intptr_t a2, intptr_t a3) {
    intptr_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return ret;
}

static inline intptr_t sys_call6(intptr_t num, intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4, intptr_t a5, intptr_t a6) {
    intptr_t ret;
    __asm__ volatile (
        "movq %5, %%r10\n"
        "movq %6, %%r8\n"
        "movq %7, %%r9\n"
        "syscall\n"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(a4), "r"(a5), "r"(a6)
        : "rcx", "r11", "r10", "r8", "r9", "memory"
    );
    return ret;
}

void linux_entry(int64_t argc, char **argv) {
    if (argc > 1) {
        cli_filename = argv[1];
    }
    int code = ectxt_main();
    sys_call1(SYS_EXIT, code);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile (
        "xorq %%rbp, %%rbp\n"
        "movq (%%rsp), %%rdi\n"
        "leaq 8(%%rsp), %%rsi\n"
        "andq $-16, %%rsp\n"
        "call linux_entry\n"
        "movq %%rax, %%rdi\n"
        "movq $60, %%rax\n"
        "syscall\n"
        "hlt\n"
        : : : "memory"
    );
}

#elif defined(__i386__)

#define SYS_EXIT    1
#define SYS_READ    3
#define SYS_WRITE   4
#define SYS_OPEN    5
#define SYS_CLOSE   6
#define SYS_IOCTL   54
#define SYS_MMAP2   192
#define SYS_MUNMAP  91

static inline intptr_t sys_call1(intptr_t num, intptr_t a1) {
    intptr_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(a1) : "memory");
    return ret;
}

static inline intptr_t sys_call2(intptr_t num, intptr_t a1, intptr_t a2) {
    intptr_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2) : "memory");
    return ret;
}

static inline intptr_t sys_call3(intptr_t num, intptr_t a1, intptr_t a2, intptr_t a3) {
    intptr_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2), "d"(a3) : "memory");
    return ret;
}

static inline intptr_t sys_call6(intptr_t num, intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4, intptr_t a5, intptr_t a6) {
    intptr_t ret;
    intptr_t args[6] = { a1, a2, a3, a4, a5, a6 };
    __asm__ volatile (
        "pushl %%ebp\n"
        "movl 0(%2), %%ebx\n"
        "movl 4(%2), %%ecx\n"
        "movl 8(%2), %%edx\n"
        "movl 12(%2), %%esi\n"
        "movl 16(%2), %%edi\n"
        "movl 20(%2), %%ebp\n"
        "int $0x80\n"
        "popl %%ebp\n"
        : "=a"(ret)
        : "a"(num), "r"(args)
        : "ebx", "ecx", "edx", "esi", "edi", "memory"
    );
    return ret;
}

void linux_entry(int argc, char **argv) {
    if (argc > 1) {
        cli_filename = argv[1];
    }
    int code = ectxt_main();
    sys_call1(SYS_EXIT, code);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile (
        "xorl %%ebp, %%ebp\n"
        "movl (%%esp), %%eax\n"
        "leal 4(%%esp), %%edx\n"
        "andl $-16, %%esp\n"
        "pushl %%edx\n"
        "pushl %%eax\n"
        "call linux_entry\n"
        "movl %%eax, %%ebx\n"
        "movl $1, %%eax\n"
        "int $0x80\n"
        "hlt\n"
        : : : "memory"
    );
}

#endif

const char *hal_get_cli_argument(void) {
    return cli_filename;
}

int hal_init(void) {
    if (sys_call3(SYS_IOCTL, 0, TCGETS, (intptr_t)&orig_termios) < 0) {
        return 0;
    }

    struct linux_termios raw = orig_termios;
    raw.c_iflag &= ~(0x01 | 0x02 | 0x10 | 0x20 | 0x100 | 0x400);
    raw.c_oflag &= ~(0x01);
    raw.c_cflag |= 0x30;
    raw.c_lflag &= ~(0x01 | 0x02 | 0x08 | 0x8000);
    raw.c_cc[6] = 1;
    raw.c_cc[5] = 0;

    sys_call3(SYS_IOCTL, 0, TCSETS, (intptr_t)&raw);
    return 1;
}

void hal_shutdown(void) {
    sys_call3(SYS_IOCTL, 0, TCSETS, (intptr_t)&orig_termios);
    hal_write("\r\n", 2);
}

void hal_get_term_size(int *cols, int *rows) {
    struct linux_winsize ws;
    if (sys_call3(SYS_IOCTL, 1, TIOCGWINSZ, (intptr_t)&ws) == 0 && ws.ws_col > 0) {
        *cols = (int)ws.ws_col;
        *rows = (int)ws.ws_row;
    } else {
        *cols = 80;
        *rows = 24;
    }
}

void hal_write(const void *buf, size_t len) {
    sys_call3(SYS_WRITE, 1, (intptr_t)buf, (intptr_t)len);
}

static int hal_read_byte(uint8_t *b) {
    intptr_t ret = sys_call3(SYS_READ, 0, (intptr_t)b, 1);
    return ret == 1;
}

int hal_read_key(void) {
    uint8_t c;
    if (!hal_read_byte(&c)) return KEY_NONE;

    if (c == 27) {
        uint8_t seq[3];
        if (!hal_read_byte(&seq[0])) return KEY_ESCAPE;
        if (!hal_read_byte(&seq[1])) return KEY_ESCAPE;

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (!hal_read_byte(&seq[2])) return KEY_ESCAPE;
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return KEY_HOME;
                        case '3': return KEY_DELETE;
                        case '4': return KEY_END;
                        case '5': return KEY_PAGE_UP;
                        case '6': return KEY_PAGE_DOWN;
                        case '7': return KEY_HOME;
                        case '8': return KEY_END;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return KEY_ARROW_UP;
                    case 'B': return KEY_ARROW_DOWN;
                    case 'C': return KEY_ARROW_RIGHT;
                    case 'D': return KEY_ARROW_LEFT;
                    case 'H': return KEY_HOME;
                    case 'F': return KEY_END;
                }
            }
        }
        return KEY_ESCAPE;
    }

    if (c == 127 || c == 8) return KEY_BACKSPACE;
    return (int)c;
}

void *hal_alloc(size_t size) {
#if defined(__x86_64__)
    intptr_t ret = sys_call6(SYS_MMAP, 0, (intptr_t)size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#else
    intptr_t ret = sys_call6(SYS_MMAP2, 0, (intptr_t)size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif
    if ((uintptr_t)ret >= (uintptr_t)-4095) return (void *)0;
    return (void *)ret;
}

void hal_free(void *ptr, size_t size) {
    if (ptr) {
        sys_call2(SYS_MUNMAP, (intptr_t)ptr, (intptr_t)size);
    }
}

int hal_file_read(const char *path, char **out_buf, size_t *out_size) {
    intptr_t fd = sys_call3(SYS_OPEN, (intptr_t)path, O_RDONLY, 0);
    if (fd < 0) return 0;

    size_t cap = 4096;
    size_t total = 0;
    char *buf = (char *)hal_alloc(cap);
    if (!buf) {
        sys_call1(SYS_CLOSE, fd);
        return 0;
    }

    while (1) {
        if (total + 1024 >= cap) {
            size_t new_cap = cap * 2;
            char *new_buf = (char *)hal_alloc(new_cap);
            if (!new_buf) {
                hal_free(buf, cap);
                sys_call1(SYS_CLOSE, fd);
                return 0;
            }
            memcpy(new_buf, buf, total);
            hal_free(buf, cap);
            buf = new_buf;
            cap = new_cap;
        }

        intptr_t rd = sys_call3(SYS_READ, fd, (intptr_t)(buf + total), 1024);
        if (rd <= 0) break;
        total += (size_t)rd;
    }

    buf[total] = '\0';
    sys_call1(SYS_CLOSE, fd);

    *out_buf = buf;
    *out_size = total;
    return 1;
}

int hal_file_write_chunks(const char *path, const void *p1, size_t n1, const void *p2, size_t n2) {
    intptr_t fd = sys_call3(SYS_OPEN, (intptr_t)path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 0;

    if (n1 > 0) {
        if (sys_call3(SYS_WRITE, fd, (intptr_t)p1, (intptr_t)n1) != (intptr_t)n1) {
            sys_call1(SYS_CLOSE, fd);
            return 0;
        }
    }

    if (n2 > 0) {
        if (sys_call3(SYS_WRITE, fd, (intptr_t)p2, (intptr_t)n2) != (intptr_t)n2) {
            sys_call1(SYS_CLOSE, fd);
            return 0;
        }
    }

    sys_call1(SYS_CLOSE, fd);
    return 1;
}