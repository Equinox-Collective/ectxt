#include "hal/hal.h"
#include "core/buffer.h"
#include "render/render.h"
#include "libc/string.h"
#include "libc/stdio.h"

void term_print(const char *str) {
    hal_write(str, strlen(str));
}

static int prompt_filename(RenderContext *render, const GapBuffer *gb, char *out_buf, size_t max_len) {
    size_t len = 0;
    out_buf[0] = '\0';

    while (1) {
        char prompt_msg[128];
        snprintf(prompt_msg, sizeof(prompt_msg), "Save as: %s_", out_buf);
        render_set_message(render, prompt_msg);
        render_update(render, gb, (void *)0, 1);

        int key = hal_read_key();
        if (key == KEY_NONE) continue;

        if (key == KEY_ENTER) {
            if (len > 0) {
                render->status_msg[0] = '\0';
                return 1;
            }
        } else if (key == KEY_ESCAPE) {
            render_set_message(render, "Save cancelled.");
            return 0;
        } else if (key == KEY_BACKSPACE) {
            if (len > 0) {
                out_buf[--len] = '\0';
            }
        } else if (key >= 32 && key <= 126 && len < max_len - 1) {
            out_buf[len++] = (char)key;
            out_buf[len] = '\0';
        }
    }
}

int ectxt_main(void) {
    if (!hal_init()) return 1;

    GapBuffer gb;
    if (!gb_init(&gb, 4096)) {
        hal_shutdown();
        return 1;
    }

    char filename[256];
    filename[0] = '\0';
    const char *cli_arg = hal_get_cli_argument();
    if (cli_arg) {
        strncpy(filename, cli_arg, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
    }

    int is_dirty = 0;
    if (filename[0]) {
        char *file_data = (void *)0;
        size_t file_size = 0;
        if (hal_file_read(filename, &file_data, &file_size)) {
            gb_load_data(&gb, file_data, file_size);
            hal_free(file_data, file_size + 1);
        }
    }

    RenderContext render;
    if (!render_init(&render)) {
        gb_free(&gb);
        hal_shutdown();
        return 1;
    }

    int cols = 0, rows = 0;
    int running = 1;

    while (running) {
        hal_get_term_size(&cols, &rows);
        render_resize(&render, cols, rows);

        render_update(&render, &gb, filename[0] ? filename : (void *)0, is_dirty);

        int key = hal_read_key();
        if (key == KEY_NONE) continue;

        switch (key) {
            case KEY_CTRL_Q:
                running = 0;
                break;
            case KEY_CTRL_S:
                if (!filename[0]) {
                    if (!prompt_filename(&render, &gb, filename, sizeof(filename))) {
                        break;
                    }
                }
                {
                    const char *p1, *p2;
                    size_t n1, n2;
                    gb_get_chunks(&gb, &p1, &n1, &p2, &n2);
                    if (hal_file_write_chunks(filename, p1, n1, p2, n2)) {
                        is_dirty = 0;
                        render_set_message(&render, "File saved!");
                    } else {
                        render_set_message(&render, "Error saving file!");
                    }
                }
                break;
            case KEY_ARROW_LEFT:  gb_move_left(&gb); break;
            case KEY_ARROW_RIGHT: gb_move_right(&gb); break;
            case KEY_ARROW_UP:    gb_move_up(&gb); break;
            case KEY_ARROW_DOWN:  gb_move_down(&gb); break;
            case KEY_BACKSPACE:
                gb_delete_back(&gb);
                is_dirty = 1;
                render.status_msg[0] = '\0';
                break;
            case KEY_DELETE:
                gb_delete_forward(&gb);
                is_dirty = 1;
                render.status_msg[0] = '\0';
                break;
            case KEY_ENTER:
                gb_insert_char(&gb, '\n');
                is_dirty = 1;
                render.status_msg[0] = '\0';
                break;
            default:
                if (key >= 32 && key <= 126) {
                    gb_insert_char(&gb, (char)key);
                    is_dirty = 1;
                    render.status_msg[0] = '\0';
                } else if (key == 9) {
                    for (int i = 0; i < 4; i++) {
                        gb_insert_char(&gb, ' ');
                    }
                    is_dirty = 1;
                    render.status_msg[0] = '\0';
                }
                break;
        }
    }

    render_free(&render);
    gb_free(&gb);
    hal_shutdown();
    return 0;
}