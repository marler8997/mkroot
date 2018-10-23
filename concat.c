#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

char *_concat(unsigned count,  ...)
{
  size_t size = 0;
  va_list args;
  va_start(args, count);
  for (unsigned i = 0; i < count; i++) {
    //length_list[i] = strlen(va_arg(args, char*));
    size += strlen(va_arg(args, char*));
  }
  va_end(args);
  char *buffer = malloc(size + 1);
  if (!buffer)
    return NULL;
  va_end(args);
  va_start(args, count);
  char *next = buffer;
  for (unsigned i = 0; i < count; i++) {
    next = stpcpy(next, va_arg(args, char*));
  }
  va_end(args);
  if (size != next - buffer) {
    fprintf(stderr, "Error: codebug in the concat function, expected size %llu but got %llun",
            (unsigned long long)size, (unsigned long long)(next-buffer));
    free(buffer);
    return NULL;
  }
  return buffer;
}

