#include "hal.h"
#include "../libc/string.h"

#ifndef SYS_EXIT
#define SYS_EXIT   1
#endif
#ifndef SYS_READ
#define SYS_READ   2
#endif
#ifndef SYS_WRITE
#define SYS_WRITE  3
#endif
#ifndef SYS_OPEN
#define SYS_OPEN   4
#endif
#ifndef SYS_CLOSE
#define SYS_CLOSE  5
#endif
#ifndef SYS_MMAP
#define SYS_MMAP   6
#endif
#ifndef SYS_MUNMAP
#define SYS_MUNMAP 7
#endif
#ifndef SYS_IOCTL
#define SYS_IOCTL  8
#endif

extern int ectxt_main(void);

static const char *os_cli_arg = (void *)0;

static char fallback_heap[131072];
static size_t fallback_heap_offset = 0;

#if defined(__x86_64__)

static inline intptr_t os_syscall1(intptr_t num, intptr_t a1) {
    intptr_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static inline intptr_t os_syscall2(intptr_t num, intptr_t a1, intptr_t a2) {
    intptr_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static inline intptr_t os_syscall3(intptr_t num, intptr_t a1, intptr_t a2, intptr_t a3) {
    intptr_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return ret;
}

static inline intptr_t os_syscall6(intptr_t num, intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4, intptr_t a5, intptr_t a6) {
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

void unknownos_entry(int64_t argc, char **argv) {
    if (argc > 1) {
        os_cli_arg = argv[1];
    }
    int code = ectxt_main();
    os_syscall1(SYS_EXIT, code);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile (
        "xorq %%rbp, %%rbp\n"
        "movq (%%rsp), %%rdi\n"
        "leaq 8(%%rsp), %%rsi\n"
        "andq $-16, %%rsp\n"
        "call unknownos_entry\n"
        "movq %%rax, %%rdi\n"
        "movq %0, %%rax\n"
        "syscall\n"
        "hlt\n"
        : : "i"(SYS_EXIT) : "memory"
    );
}

#elif defined(__i386__)

static inline intptr_t os_syscall1(intptr_t num, intptr_t a1) {
    intptr_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(a1) : "memory");
    return ret;
}

static inline intptr_t os_syscall2(intptr_t num, intptr_t a1, intptr_t a2) {
    intptr_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2) : "memory");
    return ret;
}

static inline intptr_t os_syscall3(intptr_t num, intptr_t a1, intptr_t a2, intptr_t a3) {
    intptr_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2), "d"(a3) : "memory");
    return ret;
}

static inline intptr_t os_syscall6(intptr_t num, intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4, intptr_t a5, intptr_t a6) {
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

void unknownos_entry(int argc, char **argv) {
    if (argc > 1) {
        os_cli_arg = argv[1];
    }
    int code = ectxt_main();
    os_syscall1(SYS_EXIT, code);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile (
        "xorl %%ebp, %%ebp\n"
        "movl (%%esp), %%eax\n"
        "leal 4(%%esp), %%edx\n"
        "andl $-16, %%esp\n"
        "pushl %%edx\n"
        "pushl %%eax\n"
        "call unknownos_entry\n"
        "movl %%eax, %%ebx\n"
        "movl %0, %%eax\n"
        "int $0x80\n"
        "hlt\n"
        : : "i"(SYS_EXIT) : "memory"
    );
}

#endif

const char *hal_get_cli_argument(void) {
    return os_cli_arg;
}

int hal_init(void) {
    return 1;
}

void hal_shutdown(void) {
    hal_write("\x1b[0m\x1b[?25h\r\n", 10);
}

void hal_get_term_size(int *cols, int *rows) {
    *cols = 80;
    *rows = 24;
}

void hal_write(const void *buf, size_t len) {
    os_syscall3(SYS_WRITE, 1, (intptr_t)buf, (intptr_t)len);
}

static int hal_read_byte(uint8_t *b) {
    intptr_t ret = os_syscall3(SYS_READ, 0, (intptr_t)b, 1);
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
    intptr_t ret = os_syscall6(SYS_MMAP, 0, (intptr_t)size, 0x3, 0x22, -1, 0);
    if (ret > 0 && (uintptr_t)ret < (uintptr_t)-4095) {
        return (void *)ret;
    }

    size = (size + 15) & ~15;
    if (fallback_heap_offset + size <= sizeof(fallback_heap)) {
        void *ptr = &fallback_heap[fallback_heap_offset];
        fallback_heap_offset += size;
        return ptr;
    }

    return (void *)0;
}

void hal_free(void *ptr, size_t size) {
    if (ptr >= (void *)fallback_heap && ptr < (void *)(fallback_heap + sizeof(fallback_heap))) {
        return;
    }
    if (ptr) {
        os_syscall2(SYS_MUNMAP, (intptr_t)ptr, (intptr_t)size);
    }
}

int hal_file_read(const char *path, char **out_buf, size_t *out_size) {
    intptr_t fd = os_syscall3(SYS_OPEN, (intptr_t)path, 0, 0);
    if (fd < 0) return 0;

    size_t cap = 4096;
    size_t total = 0;
    char *buf = (char *)hal_alloc(cap);
    if (!buf) {
        os_syscall1(SYS_CLOSE, fd);
        return 0;
    }

    while (1) {
        if (total + 1024 >= cap) {
            size_t new_cap = cap * 2;
            char *new_buf = (char *)hal_alloc(new_cap);
            if (!new_buf) {
                hal_free(buf, cap);
                os_syscall1(SYS_CLOSE, fd);
                return 0;
            }
            memcpy(new_buf, buf, total);
            hal_free(buf, cap);
            buf = new_buf;
            cap = new_cap;
        }

        intptr_t rd = os_syscall3(SYS_READ, fd, (intptr_t)(buf + total), 1024);
        if (rd <= 0) break;
        total += (size_t)rd;
    }

    buf[total] = '\0';
    os_syscall1(SYS_CLOSE, fd);

    *out_buf = buf;
    *out_size = total;
    return 1;
}

int hal_file_write_chunks(const char *path, const void *p1, size_t n1, const void *p2, size_t n2) {
    intptr_t fd = os_syscall3(SYS_OPEN, (intptr_t)path, 0x241, 0644);
    if (fd < 0) return 0;

    if (n1 > 0) {
        if (os_syscall3(SYS_WRITE, fd, (intptr_t)p1, (intptr_t)n1) != (intptr_t)n1) {
            os_syscall1(SYS_CLOSE, fd);
            return 0;
        }
    }

    if (n2 > 0) {
        if (os_syscall3(SYS_WRITE, fd, (intptr_t)p2, (intptr_t)n2) != (intptr_t)n2) {
            os_syscall1(SYS_CLOSE, fd);
            return 0;
        }
    }

    os_syscall1(SYS_CLOSE, fd);
    return 1;
}