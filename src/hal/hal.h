#ifndef HAL_H
#define HAL_H

#include <stddef.h>
#include <stdint.h>

enum Key {
    KEY_NONE = 0,
    KEY_CTRL_C = 3,
    KEY_CTRL_Q = 17,
    KEY_CTRL_S = 19,
    KEY_ENTER = 13,
    KEY_ESCAPE = 27,
    KEY_BACKSPACE = 127,
    KEY_ARROW_UP = 1000,
    KEY_ARROW_DOWN,
    KEY_ARROW_LEFT,
    KEY_ARROW_RIGHT,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_DELETE
};

int hal_init(void);
void hal_shutdown(void);
void hal_get_term_size(int *cols, int *rows);

int hal_read_key(void);
void hal_write(const void *buf, size_t len);

void *hal_alloc(size_t size);
void hal_free(void *ptr, size_t size);

int hal_file_read(const char *path, char **out_buf, size_t *out_size);
int hal_file_write_chunks(const char *path, const void *p1, size_t n1, const void *p2, size_t n2);

#endif