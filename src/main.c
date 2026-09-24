#include "hal/hal.h"
#include "core/buffer.h"
#include "render/render.h"
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

    RenderContext render;
    if (!render_init(&render)) {
        gb_free(&gb);
        hal_shutdown();
        return 1;
    }

    int cols = 0, rows = 0;
    hal_get_term_size(&cols, &rows);
    render_resize(&render, cols, rows);

    int running = 1;
    while (running) {
        hal_get_term_size(&cols, &rows);
        render_resize(&render, cols, rows);

        render_update(&render, &gb);

        int key = hal_read_key();
        if (key == KEY_NONE) continue;

        switch (key) {
            case KEY_CTRL_Q:
                running = 0;
                break;
            case KEY_ARROW_LEFT:
                gb_move_left(&gb);
                break;
            case KEY_ARROW_RIGHT:
                gb_move_right(&gb);
                break;
            case KEY_ARROW_UP:
                gb_move_up(&gb);
                break;
            case KEY_ARROW_DOWN:
                gb_move_down(&gb);
                break;
            case KEY_BACKSPACE:
                gb_delete_back(&gb);
                break;
            case KEY_DELETE:
                gb_delete_forward(&gb);
                break;
            case KEY_ENTER:
                gb_insert_char(&gb, '\n');
                break;
            default:
                if (key >= 32 && key <= 126) {
                    gb_insert_char(&gb, (char)key);
                } else if (key == 9) {
                    for (int i = 0; i < 4; i++) {
                        gb_insert_char(&gb, ' ');
                    }
                }
                break;
        }
    }

    render_free(&render);
    gb_free(&gb);
    hal_shutdown();
    return 0;
}

void mainCRTStartup(void) {
    int ret = ectxt_main();
    ExitProcess((uint32_t)ret);
}