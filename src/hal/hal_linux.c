#include "hal.h"
#include "../libc/string.h"

#define SYS_READ    0
#define SYS_WRITE   1
#define SYS_OPEN    2
#define SYS_CLOSE   3
#define SYS_MMAP    9
#define SYS_MUNMAP  11
#define SYS_IOCTL   16
#define SYS_EXIT    60

#define TCGETS      0x5401
#define TCSETSF     0x5404
#define TIOCGWINSZ  0x5413

#define O_RDONLY    0
#define O_WRONLY    1
#define O_CREAT     0x40
#define O_TRUNC     0x200

#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define MAP_PRIVATE 0x02
#define MAP_ANON    0x20

struct linux_termios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t  c_line;
    uint8_t  c_cc[32];
    uint32_t c_ispeed;
    uint32_t c_ospeed;
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

static inline int64_t sys_call1(int64_t num, int64_t a1) {
    int64_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static inline int64_t sys_call2(int64_t num, int64_t a1, int64_t a2) {
    int64_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static inline int64_t sys_call3(int64_t num, int64_t a1, int64_t a2, int64_t a3) {
    int64_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return ret;
}

static inline int64_t sys_call6(int64_t num, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5, int64_t a6) {
    int64_t ret;
    register int64_t r10 __asm__("r10") = a4;
    register int64_t r8  __asm__("r8")  = a5;
    register int64_t r9  __asm__("r9")  = a6;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

const char *hal_get_cli_argument(void) {
    return cli_filename;
}

int hal_init(void) {
    if (sys_call3(SYS_IOCTL, 0, TCGETS, (int64_t)&orig_termios) < 0) {
        return 0;
    }

    struct linux_termios raw = orig_termios;
    raw.c_iflag &= ~(0x01 | 0x02 | 0x10 | 0x20 | 0x100 | 0x400);
    raw.c_oflag &= ~(0x01);
    raw.c_cflag |= 0x30;
    raw.c_lflag &= ~(0x01 | 0x02 | 0x08 | 0x8000);
    raw.c_cc[6] = 1;
    raw.c_cc[5] = 0;

    sys_call3(SYS_IOCTL, 0, TCSETSF, (int64_t)&raw);
    return 1;
}

void hal_shutdown(void) {
    sys_call3(SYS_IOCTL, 0, TCSETSF, (int64_t)&orig_termios);
}

void hal_get_term_size(int *cols, int *rows) {
    struct linux_winsize ws;
    if (sys_call3(SYS_IOCTL, 1, TIOCGWINSZ, (int64_t)&ws) == 0 && ws.ws_col > 0) {
        *cols = (int)ws.ws_col;
        *rows = (int)ws.ws_row;
    } else {
        *cols = 80;
        *rows = 24;
    }
}

void hal_write(const void *buf, size_t len) {
    sys_call3(SYS_WRITE, 1, (int64_t)buf, (int64_t)len);
}

static int hal_read_byte(uint8_t *b) {
    int64_t ret = sys_call3(SYS_READ, 0, (int64_t)b, 1);
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
    int64_t ret = sys_call6(SYS_MMAP, 0, (int64_t)size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ret < 0) return (void *)0;
    return (void *)ret;
}

void hal_free(void *ptr, size_t size) {
    if (ptr) {
        sys_call2(SYS_MUNMAP, (int64_t)ptr, (int64_t)size);
    }
}

int hal_file_read(const char *path, char **out_buf, size_t *out_size) {
    int64_t fd = sys_call3(SYS_OPEN, (int64_t)path, O_RDONLY, 0);
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

        int64_t rd = sys_call3(SYS_READ, fd, (int64_t)(buf + total), 1024);
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
    int64_t fd = sys_call3(SYS_OPEN, (int64_t)path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 0;

    if (n1 > 0) {
        if (sys_call3(SYS_WRITE, fd, (int64_t)p1, (int64_t)n1) != (int64_t)n1) {
            sys_call1(SYS_CLOSE, fd);
            return 0;
        }
    }

    if (n2 > 0) {
        if (sys_call3(SYS_WRITE, fd, (int64_t)p2, (int64_t)n2) != (int64_t)n2) {
            sys_call1(SYS_CLOSE, fd);
            return 0;
        }
    }

    sys_call1(SYS_CLOSE, fd);
    return 1;
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
        "pop %rdi\n"
        "mov %rsp, %rsi\n"
        "call linux_entry\n"
    );
}