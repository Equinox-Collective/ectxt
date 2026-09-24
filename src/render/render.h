#ifndef RENDER_H
#define RENDER_H

#include <stddef.h>
#include "../core/buffer.h"

typedef struct {
    int screen_cols;
    int screen_rows;
    size_t row_offset;
    size_t col_offset;
    char *frame_buf;
    size_t frame_cap;
} RenderContext;

int render_init(RenderContext *ctx);
void render_free(RenderContext *ctx);
void render_resize(RenderContext *ctx, int cols, int rows);
void render_update(RenderContext *ctx, const GapBuffer *gb);

#endif