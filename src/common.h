#ifndef MKBOOTIMAGE_COMMON_H
#define MKBOOTIMAGE_COMMON_H

int errorf(const char *fmt, ...);
uint32_t calc_checksum(uint32_t *, uint32_t *);
int is_postfix(char *, char *);
int is_on_list(char **, char *);

#endif
