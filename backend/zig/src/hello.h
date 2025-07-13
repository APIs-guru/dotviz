#include <gvc.h>
GVC_t *viz_create_context(void);
int hello(GVC_t *gvc, const char *dot_string, char *outputBuf,  size_t bufSize, size_t *writtenLen);