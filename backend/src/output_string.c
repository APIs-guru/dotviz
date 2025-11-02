#include "output_string.h"
#include "cgraph.h"
#include "types.h"  // IWYU pragma: keep
#include "geom.h"   // IWYU pragma: keep
#include "gvcjob.h" // IWYU pragma: keep
#include <stdlib.h>
#include <string.h>

void out_put(output_string *output, const char *str, size_t len) {
  if (str == NULL || len == 0)
    return;

  if (len > output->data_allocated - (output->data_position + 1)) {
    /* ensure enough allocation for string = null terminator */
    output->data_allocated = output->data_position + len + 1;
    output->data = realloc(output->data, output->data_allocated);
    if (!output->data) {
      agerrorf("memory allocation failure\n");
      exit(1);
    }
  }
  memcpy(output->data + output->data_position, str, len);
  output->data_position += len;
  output->data[output->data_position] = '\0'; /* keep null terminated */
}

void out_puts(output_string *output, const char *str) {
  size_t len = str != NULL ? strlen(str) : 0;

  out_put(output, str, len);
}

void out_putc(output_string *output, const char c) { out_put(output, &c, 1); }

output_string job2output_string(GVJ_t *job) {
  output_string output;
  output.data_allocated = job->output_data_allocated;
  output.data_position = job->output_data_position;
  output.data = job->output_data;
  return output;
}

void output_string2job(GVJ_t *job, output_string *output) {
  job->output_data_allocated = output->data_allocated;
  job->output_data_position = output->data_position;
  job->output_data = output->data;
}
