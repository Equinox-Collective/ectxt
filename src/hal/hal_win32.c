#include "hal.h"

#define STD_INPUT_HANDLE  ((uint32_t)-10)
#define STD_OUTPUT_HANDLE ((uint32_t)-11)

#define ENABLE_PROCESSED_INPUT        0x0001
#define ENABLE_LINE_INPUT             0x0002
#define ENABLE_ECHO_INPUT             0x0004
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200

#define ENABLE_PROCESSED_OUTPUT            0x0001
#define ENABLE_WRAP_AT_EOL_OUTPUT          0x0002
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004

#define MEM_COMMIT   0x00001000
#define MEM_RESERVE  0x00002000
#define MEM_RELEASE  0x00008000
#define PAGE_READWRITE 0x04

typedef struct {
    int16_t x;
    int16_t y;
} COORD;

typedef struct {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
} SMALL_RECT;

typedef struct {
    COORD dwSize;
    COORD dwCursorPosition;
    uint16_t wAttributes;
    SMALL_RECT srWindow;
    COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO;

__declspec(dllimport) void * __stdcall GetStdHandle(uint32_t nStdHandle);
__declspec(dllimport) int    __stdcall GetConsoleMode(void *hConsoleHandle, uint32_t *lpMode);
__declspec(dllimport) int    __stdcall SetConsoleMode(void *hConsoleHandle, uint32_t dwMode);
__declspec(dllimport) int    __stdcall GetConsoleScreenBufferInfo(void *hConsoleHandle, CONSOLE_SCREEN_BUFFER_INFO *lpInfo);
__declspec(dllimport) int    __stdcall ReadFile(void *hFile, void *lpBuffer, uint32_t nNumberOfBytesToRead, uint32_t *lpNumberOfBytesRead, void *lpOverlapped);
__declspec(dllimport) int    __stdcall WriteFile(void *hFile, const void *lpBuffer, uint32_t nNumberOfBytesToWrite, uint32_t *lpNumberOfBytesWritten, void *lpOverlapped);
__declspec(dllimport) void * __stdcall VirtualAlloc(void *lpAddress, size_t dwSize, uint32_t flAllocationType, uint32_t flProtect);
__declspec(dllimport) int    __stdcall VirtualFree(void *lpAddress, size_t dwSize, uint32_t dwFreeType);

static void *h_stdin;
static void *h_stdout;
static uint32_t orig_in_mode;
static uint32_t orig_out_mode;

int hal_init(void) {
    h_stdin = GetStdHandle(STD_INPUT_HANDLE);
    h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);

    if (!h_stdin || !h_stdout) {
        return 0;
    }

    if (!GetConsoleMode(h_stdin, &orig_in_mode) || !GetConsoleMode(h_stdout, &orig_out_mode)) {
        return 0;
    }

    uint32_t raw_in = orig_in_mode;
    raw_in &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    raw_in |= ENABLE_VIRTUAL_TERMINAL_INPUT;

    uint32_t raw_out = orig_out_mode;
    raw_out |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    SetConsoleMode(h_stdin, raw_in);
    SetConsoleMode(h_stdout, raw_out);

    return 1;
}

void hal_shutdown(void) {
    SetConsoleMode(h_stdin, orig_in_mode);
    SetConsoleMode(h_stdout, orig_out_mode);
}

void hal_get_term_size(int *cols, int *rows) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(h_stdout, &csbi)) {
        *cols = csbi.srWindow.right - csbi.srWindow.left + 1;
        *rows = csbi.srWindow.bottom - csbi.srWindow.top + 1;
    } else {
        *cols = 80;
        *rows = 24;
    }
}

void hal_write(const void *buf, size_t len) {
    uint32_t written;
    WriteFile(h_stdout, buf, (uint32_t)len, &written, (void *)0);
}

static int hal_read_byte(uint8_t *b) {
    uint32_t read = 0;
    if (!ReadFile(h_stdin, b, 1, &read, (void *)0) || read == 0) {
        return 0;
    }
    return 1;
}

int hal_read_key(void) {
    uint8_t c;
    if (!hal_read_byte(&c)) {
        return KEY_NONE;
    }

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

    if (c == 8) return KEY_BACKSPACE;

    return (int)c;
}

void *hal_alloc(size_t size) {
    return VirtualAlloc((void *)0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void hal_free(void *ptr, size_t size) {
    (void)size;
    if (ptr) {
        VirtualFree(ptr, 0, MEM_RELEASE);
    }
}