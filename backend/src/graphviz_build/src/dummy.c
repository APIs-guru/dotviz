// HACK: for linking purposes

#include <stdlib.h>
#include <sys/types.h>

int getpid() { return 0; }

int64_t times(int a) { return 0; }

void start_timer() {}

double elapsed_sec() {return 0;}


void *mmap(void *addr, size_t length, int prot, int flags, int fd,
           off_t offset) {
  return NULL;
}

int munmap(void *addr, size_t length) { return 0; }
