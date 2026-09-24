#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *data;
    size_t cap;
    size_t gap_start;
    size_t gap_end;
} GapBuffer;

int gb_init(GapBuffer *gb, size_t initial_cap);
int gb_load_data(GapBuffer *gb, const char *data, size_t len);
void gb_free(GapBuffer *gb);

size_t gb_length(const GapBuffer *gb);
size_t gb_cursor(const GapBuffer *gb);
char gb_char_at(const GapBuffer *gb, size_t index);

void gb_insert_char(GapBuffer *gb, char c);
void gb_delete_back(GapBuffer *gb);
void gb_delete_forward(GapBuffer *gb);

void gb_set_cursor(GapBuffer *gb, size_t new_pos);
void gb_move_left(GapBuffer *gb);
void gb_move_right(GapBuffer *gb);
void gb_move_up(GapBuffer *gb);
void gb_move_down(GapBuffer *gb);

void gb_get_cursor_coords(const GapBuffer *gb, size_t *row, size_t *col);

void gb_get_chunks(const GapBuffer *gb, const char **p1, size_t *n1, const char **p2, size_t *n2);

#endif