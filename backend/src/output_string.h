#ifndef OUTPUT_STRING_H
#define OUTPUT_STRING_H

#include <stddef.h>
typedef struct {
  char *data;
  size_t data_allocated;
  size_t data_position;
} output_string;

void out_strput(output_string *output, char *str);

#endif
