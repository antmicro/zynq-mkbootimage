#ifndef MKBOOTIMAGE_COMMON_H
#define MKBOOTIMAGE_COMMON_H

typedef enum error
{
  /* The job was ended sucessfully */
  SUCCESS = 0,

  /* Common error codes */
  ERROR_NOMEM = 1,
  ERROR_CANT_READ,
  ERROR_CANT_WRITE,
  ERROR_ITERATION_END,

  /* BIF parser specific errors */
  ERROR_BIF_NOFILE,
  ERROR_BIF_PARSER,
  ERROR_BIF_UNSUPPORTED_ATTR,
  ERROR_BIF_UNINITIALIZED,
  ERROR_BIF_UNSUPPORTED_VAL,
  ERROR_BIF_LEXER,

  /* Bootrom compiler specific errors */
  ERROR_BOOTROM_NOFILE,
  ERROR_BOOTROM_BITSTREAM,
  ERROR_BOOTROM_ELF,
  ERROR_BOOTROM_SEC_OVERLAP,
  ERROR_BOOTROM_UNSUPPORTED,
  ERROR_BOOTROM_NOMEM,

  /* Bootrom parser specific errors */
  ERROR_BIN_FILE_EXISTS,
  ERROR_BIN_NOFILE,
  ERROR_BIN_WADDR,
} error;

int errorf(const char *fmt, ...);
uint32_t calc_checksum(uint32_t *, uint32_t *);
bool is_postfix(char *, char *);
bool is_on_list(char **, char *);

#endif
