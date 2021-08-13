#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <common.h>

int errorf(const char *fmt, ...) {
  int n;
  va_list args;

  va_start(args, fmt);
  fprintf(stderr, "error: ");
  n = vfprintf(stderr, fmt, args);
  va_end(args);

  return n;
}

uint32_t calc_checksum(uint32_t *start_addr, uint32_t *end_addr) {
  uint32_t *ptr;
  uint32_t sum;

  sum = 0;
  ptr = start_addr;

  while(ptr <= end_addr) {
    sum += *ptr;
    ptr++;
  }

  return ~sum;
}

/* Check if pfix is a postifx of string */
int is_postfix(char *string, char *pfix) {
  return strcmp(string+strlen(string)-strlen(pfix), pfix) == 0;
}

/* Check if a string is present on a list*/
int is_on_list(char *list[], char *s) {
  int i;

  for (i = 0; list[i]; i++)
    if (strcmp(list[i], s) == 0)
      return 0xFF;
  return 0;
}
