#include "hal/hal.h"
#include "libc/stdio.h"
#include "libc/string.h"

__declspec(dllimport) void __stdcall ExitProcess(uint32_t uExitCode);

void __main(void) {}

void term_print(const char *str) {
    hal_write(str, strlen(str));
}

int ectxt_main(void) {
    if (!hal_init()) {
        return 1;
    }

    int cols = 0, rows = 0;
    hal_get_term_size(&cols, &rows);

    hal_write("\x1b[2J\x1b[H", 7);

    char msg[128];
    snprintf(msg, sizeof(msg), "ECTXT HAL Test | Screen: %dx%d | Press Ctrl+Q to exit.\r\n", cols, rows);
    hal_write(msg, strlen(msg));

    while (1) {
        int key = hal_read_key();
        if (key == KEY_NONE) {
            continue;
        }

        if (key == KEY_CTRL_Q) {
            break;
        }

        char kbuf[64];
        if (key >= 1000) {
            snprintf(kbuf, sizeof(kbuf), "Special Key Pressed: %d\r\n", key);
        } else if (key >= 32 && key <= 126) {
            snprintf(kbuf, sizeof(kbuf), "Char Pressed: '%c' (ASCII %d)\r\n", (char)key, key);
        } else {
            snprintf(kbuf, sizeof(kbuf), "Control Code: %d\r\n", key);
        }
        hal_write(kbuf, strlen(kbuf));
    }

    hal_write("\x1b[2J\x1b[H", 7);
    hal_shutdown();
    return 0;
}

void mainCRTStartup(void) {
    int ret = ectxt_main();
    ExitProcess((uint32_t)ret);
}