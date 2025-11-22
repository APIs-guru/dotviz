#ifndef OUTPUT_STRING_H
#define OUTPUT_STRING_H

#include <stddef.h>
typedef struct output_string_s {
  char *data;
  size_t data_allocated;
  size_t data_position;
} output_string;

void out_put(output_string *output, const char *str, size_t len);
void out_puts(output_string *output, const char *str);
void out_putc(output_string *output, char c);

#endif
