#ifndef NEDIT_MALLOC_H_
#define NEDIT_MALLOC_H_

#include <stddef.h>

void *NEditMalloc(size_t size);
void *NEditCalloc(size_t nmemb, size_t size);
void *NEditRealloc(void *ptr, size_t new_size);
void NEditFree(void *ptr);
char *NEditStrdup(const char *str);
#define NEditNew(type) ((type *) NEditMalloc(sizeof(type)))

#endif
