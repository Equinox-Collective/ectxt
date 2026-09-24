#include "buffer.h"
#include "../hal/hal.h"
#include "../libc/string.h"

#define GAP_DEFAULT_CAP 4096

static int gb_grow(GapBuffer *gb, size_t min_cap) {
    size_t new_cap = gb->cap ? gb->cap * 2 : GAP_DEFAULT_CAP;
    while (new_cap < min_cap) {
        new_cap *= 2;
    }

    char *new_data = (char *)hal_alloc(new_cap);
    if (!new_data) {
        return 0;
    }

    if (gb->data) {
        memcpy(new_data, gb->data, gb->gap_start);

        size_t suffix_len = gb->cap - gb->gap_end;
        size_t new_gap_end = new_cap - suffix_len;

        memcpy(new_data + new_gap_end, gb->data + gb->gap_end, suffix_len);
        hal_free(gb->data, gb->cap);

        gb->gap_end = new_gap_end;
    } else {
        gb->gap_start = 0;
        gb->gap_end = new_cap;
    }

    gb->data = new_data;
    gb->cap = new_cap;
    return 1;
}

int gb_init(GapBuffer *gb, size_t initial_cap) {
    gb->data = (void *)0;
    gb->cap = 0;
    gb->gap_start = 0;
    gb->gap_end = 0;
    return gb_grow(gb, initial_cap > 0 ? initial_cap : GAP_DEFAULT_CAP);
}

int gb_load_data(GapBuffer *gb, const char *data, size_t len) {
    gb_free(gb);
    if (!gb_init(gb, len + GAP_DEFAULT_CAP)) {
        return 0;
    }

    size_t clean_len = 0;
    char *dest = gb->data + (gb->cap - len);

    for (size_t i = 0; i < len; i++) {
        if (data[i] != '\r') {
            dest[clean_len++] = data[i];
        }
    }

    if (clean_len < len) {
        memmove(gb->data + (gb->cap - clean_len), dest, clean_len);
    }

    gb->gap_start = 0;
    gb->gap_end = gb->cap - clean_len;
    return 1;
}

void gb_get_chunks(const GapBuffer *gb, const char **p1, size_t *n1, const char **p2, size_t *n2) {
    *p1 = gb->data;
    *n1 = gb->gap_start;
    *p2 = gb->data + gb->gap_end;
    *n2 = gb->cap - gb->gap_end;
}

void gb_free(GapBuffer *gb) {
    if (gb->data) {
        hal_free(gb->data, gb->cap);
        gb->data = (void *)0;
    }
    gb->cap = 0;
    gb->gap_start = 0;
    gb->gap_end = 0;
}

size_t gb_length(const GapBuffer *gb) {
    return gb->cap - (gb->gap_end - gb->gap_start);
}

size_t gb_cursor(const GapBuffer *gb) {
    return gb->gap_start;
}

char gb_char_at(const GapBuffer *gb, size_t index) {
    if (index < gb->gap_start) {
        return gb->data[index];
    }
    return gb->data[index + (gb->gap_end - gb->gap_start)];
}

void gb_set_cursor(GapBuffer *gb, size_t new_pos) {
    size_t len = gb_length(gb);
    if (new_pos > len) {
        new_pos = len;
    }

    if (new_pos < gb->gap_start) {
        size_t count = gb->gap_start - new_pos;
        memmove(gb->data + gb->gap_end - count, gb->data + new_pos, count);
        gb->gap_start -= count;
        gb->gap_end -= count;
    } else if (new_pos > gb->gap_start) {
        size_t count = new_pos - gb->gap_start;
        memmove(gb->data + gb->gap_start, gb->data + gb->gap_end, count);
        gb->gap_start += count;
        gb->gap_end += count;
    }
}

void gb_insert_char(GapBuffer *gb, char c) {
    if (gb->gap_start == gb->gap_end) {
        if (!gb_grow(gb, gb->cap + 1)) {
            return;
        }
    }
    gb->data[gb->gap_start++] = c;
}

void gb_delete_back(GapBuffer *gb) {
    if (gb->gap_start > 0) {
        gb->gap_start--;
    }
}

void gb_delete_forward(GapBuffer *gb) {
    if (gb->gap_end < gb->cap) {
        gb->gap_end++;
    }
}

void gb_move_left(GapBuffer *gb) {
    if (gb->gap_start > 0) {
        gb_set_cursor(gb, gb->gap_start - 1);
    }
}

void gb_move_right(GapBuffer *gb) {
    if (gb->gap_start < gb_length(gb)) {
        gb_set_cursor(gb, gb->gap_start + 1);
    }
}

static size_t gb_find_line_start(const GapBuffer *gb, size_t pos) {
    while (pos > 0) {
        if (gb_char_at(gb, pos - 1) == '\n') {
            return pos;
        }
        pos--;
    }
    return 0;
}

static size_t gb_find_line_end(const GapBuffer *gb, size_t pos) {
    size_t len = gb_length(gb);
    while (pos < len) {
        if (gb_char_at(gb, pos) == '\n') {
            return pos;
        }
        pos++;
    }
    return len;
}

void gb_move_up(GapBuffer *gb) {
    if (gb->gap_start == 0) return;

    size_t curr_start = gb_find_line_start(gb, gb->gap_start);
    if (curr_start == 0) return;

    size_t col = gb->gap_start - curr_start;
    size_t prev_end = curr_start - 1;
    size_t prev_start = gb_find_line_start(gb, prev_end);
    size_t prev_len = prev_end - prev_start;

    if (col > prev_len) {
        col = prev_len;
    }

    gb_set_cursor(gb, prev_start + col);
}

void gb_move_down(GapBuffer *gb) {
    size_t len = gb_length(gb);
    size_t curr_start = gb_find_line_start(gb, gb->gap_start);
    size_t curr_end = gb_find_line_end(gb, gb->gap_start);

    if (curr_end >= len) return;

    size_t col = gb->gap_start - curr_start;
    size_t next_start = curr_end + 1;
    size_t next_end = gb_find_line_end(gb, next_start);
    size_t next_len = next_end - next_start;

    if (col > next_len) {
        col = next_len;
    }

    gb_set_cursor(gb, next_start + col);
}

void gb_get_cursor_coords(const GapBuffer *gb, size_t *row, size_t *col) {
    size_t r = 0;
    size_t line_start = 0;
    for (size_t i = 0; i < gb->gap_start; i++) {
        if (gb_char_at(gb, i) == '\n') {
            r++;
            line_start = i + 1;
        }
    }
    *row = r;
    *col = gb->gap_start - line_start;
}