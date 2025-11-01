#include "output_string.h"
#include "cgraph.h"
#include <stdlib.h>
#include <string.h>

void out_strput(output_string *output, char *str) {
  size_t len = str != NULL ? strlen(str) : 0;

  if (len == 0)
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
