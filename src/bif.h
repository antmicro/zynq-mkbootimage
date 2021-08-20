#ifndef BIF_PARSER_H
#define BIF_PARSER_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>
#include <linux/limits.h>

#define BIF_ARCH_ZYNQ   (1 << 0)
#define BIF_ARCH_ZYNQMP (1 << 1)

enum token_type
{
  TOKEN_EOF = 0,

  TOKEN_UNKNOWN = 256, /* skip ASCII */
  TOKEN_NAME,
};

typedef struct lexer_t {
  FILE *file; /* the file being parsed */
  char *fname;

  int line, column;

  int type; /* type of the last token read */
  struct {
    char *buffer; /* tokens of the token */
    int len, cap; /* current length and allocated size */
  };
} lexer_t;

typedef struct bif_node_t {
  char fname[PATH_MAX];

  /* supported common attributes */
  uint8_t bootloader; /* boolean */
  uint32_t load;
  uint32_t offset;
  uint32_t partition_owner;

  /* supported zynqmp attributes */
  uint8_t fsbl_config; /* boolean */
  uint8_t pmufw_image; /* boolean */
  uint32_t destination_device;
  uint32_t destination_cpu;
  uint32_t exception_level;

  /* special, non-bootgen features */
  uint8_t is_file; /* for now equal to !fsbl_config */
  uint8_t numbits;
} bif_node_t;

typedef struct bif_cfg_t {
  uint8_t arch;

  uint16_t nodes_num;
  uint16_t nodes_avail;

  bif_node_t *nodes;
} bif_cfg_t;

error init_bif_cfg(bif_cfg_t *cfg);
error deinit_bif_cfg(bif_cfg_t *cfg);

error bif_cfg_add_node(bif_cfg_t *cfg, bif_node_t *node);
error bif_node_set_attr(
  lexer_t *lex, bif_cfg_t *cfg, bif_node_t *node, char *attr_name, char *value);

error bif_parse(const char *fname, bif_cfg_t *cfg);

#endif /* BIF_PARSER_H */
