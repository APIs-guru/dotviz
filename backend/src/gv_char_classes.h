#ifndef GV_CHAR_H
#define GV_CHAR_H

#include <stdbool.h>
static inline bool gv_islower(int c) { return c >= 'a' && c <= 'z'; }

static inline bool gv_isupper(int c) { return c >= 'A' && c <= 'Z'; }

static inline bool gv_isalpha(int c) { return gv_islower(c) || gv_isupper(c); }

static inline bool gv_isdigit(int c) { return c >= '0' && c <= '9'; }

static inline bool gv_isalnum(int c) { return gv_isalpha(c) || gv_isdigit(c); }

#endif