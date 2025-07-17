#ifndef HELLO_H_
#define HELLO_H_
#include <gvc.h>
#include <stdint.h>

GVC_t *create_context(void);
uintptr_t read_graph_from_string(const char *dot_string);
int render_graph_to_svg(GVC_t *gvc, uintptr_t graphptr, char *outputBuf,
                        size_t bufSize, size_t *writtenLen);
int layout_graph(GVC_t *gvc, uintptr_t graphptr);
bool layout_done(uintptr_t graphptr);
#endif