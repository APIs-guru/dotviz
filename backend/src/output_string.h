#ifndef OUTPUT_STRING_H
#define OUTPUT_STRING_H

#include <stddef.h>
typedef struct {
  char *data;
  size_t data_allocated;
  size_t data_position;
} output_string;

size_t out_put(output_string *output, const char *str, size_t len);
void out_strput(output_string *output, const char *str);

typedef struct GVJ_s GVJ_t;
output_string job2output_string(GVJ_t *job);
void output_string2job(GVJ_t *job, output_string *output);

#endif
