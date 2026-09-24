#include "render.h"
#include "../hal/hal.h"
#include "../libc/string.h"
#include "../libc/stdio.h"

#define FRAME_BUFFER_SIZE 65536

int render_init(RenderContext *ctx) {
    ctx->screen_cols = 80;
    ctx->screen_rows = 24;
    ctx->row_offset = 0;
    ctx->col_offset = 0;
    ctx->frame_cap = FRAME_BUFFER_SIZE;
    ctx->frame_buf = (char *)hal_alloc(ctx->frame_cap);

    if (!ctx->frame_buf) {
        return 0;
    }

    hal_write("\x1b[?1049h\x1b[?25l", 14);
    return 1;
}

void render_free(RenderContext *ctx) {
    if (ctx->frame_buf) {
        hal_write("\x1b[?25h\x1b[?1049l", 14);
        hal_free(ctx->frame_buf, ctx->frame_cap);
        ctx->frame_buf = (void *)0;
    }
}

void render_resize(RenderContext *ctx, int cols, int rows) {
    ctx->screen_cols = cols;
    ctx->screen_rows = rows;
}

static void buf_append(char *dest, size_t *len, size_t max_cap, const char *src, size_t src_len) {
    if (*len + src_len >= max_cap) {
        return;
    }
    memcpy(dest + *len, src, src_len);
    *len += src_len;
}

void render_set_message(RenderContext *ctx, const char *msg) {
    strncpy(ctx->status_msg, msg, sizeof(ctx->status_msg) - 1);
    ctx->status_msg[sizeof(ctx->status_msg) - 1] = '\0';
}

void render_update(RenderContext *ctx, const GapBuffer *gb, const char *filename, int is_dirty) {
    size_t cur_row = 0, cur_col = 0;
    gb_get_cursor_coords(gb, &cur_row, &cur_col);

    int text_rows = ctx->screen_rows - 1;
    if (text_rows < 1) text_rows = 1;

    if (cur_row < ctx->row_offset) {
        ctx->row_offset = cur_row;
    }
    if (cur_row >= ctx->row_offset + (size_t)text_rows) {
        ctx->row_offset = cur_row - (size_t)text_rows + 1;
    }

    if (cur_col < ctx->col_offset) {
        ctx->col_offset = cur_col;
    }
    if (cur_col >= ctx->col_offset + (size_t)ctx->screen_cols) {
        ctx->col_offset = cur_col - (size_t)ctx->screen_cols + 1;
    }

    size_t flen = 0;
    char *fb = ctx->frame_buf;

    buf_append(fb, &flen, ctx->frame_cap, "\x1b[?25l\x1b[H", 9);

    size_t buf_len = gb_length(gb);
    size_t current_line = 0;
    size_t idx = 0;

    while (idx < buf_len && current_line < ctx->row_offset) {
        if (gb_char_at(gb, idx) == '\n') {
            current_line++;
        }
        idx++;
    }

    for (int r = 0; r < text_rows; r++) {
        size_t line_char_idx = 0;
        char line_chars[256];
        size_t line_char_count = 0;

        while (idx < buf_len) {
            char c = gb_char_at(gb, idx);
            if (c == '\n') {
                idx++;
                break;
            }
            if (line_char_idx >= ctx->col_offset && line_char_count < (size_t)ctx->screen_cols) {
                line_chars[line_char_count++] = c;
            }
            line_char_idx++;
            idx++;
        }

        if (line_char_count > 0) {
            buf_append(fb, &flen, ctx->frame_cap, line_chars, line_char_count);
        }

        buf_append(fb, &flen, ctx->frame_cap, "\x1b[K\r\n", 5);
        current_line++;
    }

    char status[160];
    int status_len = snprintf(status, sizeof(status), 
        "\x1b[7m %s%s | Ln %d, Col %d | %d bytes | %s\x1b[K\x1b[0m", 
        filename ? filename : "[No Name]",
        is_dirty ? " *" : "",
        (int)cur_row + 1, (int)cur_col + 1, (int)buf_len,
        ctx->status_msg[0] ? ctx->status_msg : "Ctrl+S: Save | Ctrl+Q: Exit");
    buf_append(fb, &flen, ctx->frame_cap, status, (size_t)status_len);

    char cursor_pos[32];
    int cursor_y = (int)(cur_row - ctx->row_offset) + 1;
    int cursor_x = (int)(cur_col - ctx->col_offset) + 1;
    int cpos_len = snprintf(cursor_pos, sizeof(cursor_pos), "\x1b[%d;%dH\x1b[?25h", cursor_y, cursor_x);
    buf_append(fb, &flen, ctx->frame_cap, cursor_pos, (size_t)cpos_len);

    hal_write(fb, flen);
}