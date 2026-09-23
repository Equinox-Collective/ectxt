#include "hal/hal.h"
#include "core/buffer.h"
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

    GapBuffer gb;
    if (!gb_init(&gb, 4096)) {
        hal_shutdown();
        return 1;
    }

    int cols = 0, rows = 0;
    hal_get_term_size(&cols, &rows);

    while (1) {
        hal_write("\x1b[H\x1b[2J", 7);

        size_t len = gb_length(&gb);
        for (size_t i = 0; i < len; i++) {
            char c = gb_char_at(&gb, i);
            if (c == '\n') {
                hal_write("\r\n", 2);
            } else {
                hal_write(&c, 1);
            }
        }

        size_t cur_row = 0, cur_col = 0;
        gb_get_cursor_coords(&gb, &cur_row, &cur_col);

        char status[128];
        snprintf(status, sizeof(status), "\x1b[%d;1H\x1b[7m[ECTXT] Ln %d, Col %d | Total Bytes: %d | Ctrl+Q to Exit\x1b[0m", 
                 rows, (int)cur_row + 1, (int)cur_col + 1, (int)len);
        hal_write(status, strlen(status));

        char cursor_jump[32];
        snprintf(cursor_jump, sizeof(cursor_jump), "\x1b[%d;%dH", (int)cur_row + 1, (int)cur_col + 1);
        hal_write(cursor_jump, strlen(cursor_jump));

        int key = hal_read_key();
        if (key == KEY_NONE) continue;
        if (key == KEY_CTRL_Q) break;

        switch (key) {
            case KEY_ARROW_LEFT:  gb_move_left(&gb); break;
            case KEY_ARROW_RIGHT: gb_move_right(&gb); break;
            case KEY_ARROW_UP:    gb_move_up(&gb); break;
            case KEY_ARROW_DOWN:  gb_move_down(&gb); break;
            case KEY_BACKSPACE:   gb_delete_back(&gb); break;
            case KEY_DELETE:      gb_delete_forward(&gb); break;
            case KEY_ENTER:       gb_insert_char(&gb, '\n'); break;
            default:
                if (key >= 32 && key <= 126) {
                    gb_insert_char(&gb, (char)key);
                } else if (key == 9) {
                    gb_insert_char(&gb, ' ');
                    gb_insert_char(&gb, ' ');
                    gb_insert_char(&gb, ' ');
                    gb_insert_char(&gb, ' ');
                }
                break;
        }
    }

    gb_free(&gb);
    hal_write("\x1b[2J\x1b[H", 7);
    hal_shutdown();
    return 0;
}

void mainCRTStartup(void) {
    int ret = ectxt_main();
    ExitProcess((uint32_t)ret);
}