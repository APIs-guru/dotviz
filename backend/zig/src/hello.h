#include <gvc.h>
#include <stdlib.h>
GVC_t *create_context(void);
int hello(GVC_t *gvc, const char *dot_string, char *outputBuf,  size_t bufSize, size_t *writtenLen);
int render_graph_to_svg(GVC_t *gvc, uintptr_t graphptr, char *outputBuf,
                        size_t bufSize, size_t *writtenLen);