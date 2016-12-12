#ifndef BIF_PARSER_H
#define BIF_PARSER_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define BIF_MAX_NODES_NUM 10

#define BIF_SUCCESS                0
#define BIF_ERROR_NOFILE           1
#define BIF_ERROR_PARSER           2
#define BIF_ERROR_UNSUPORTED_ATTR  3

#define BIF_ARCH_ZYNQ              (1 << 0)
#define BIF_ARCH_ZYNQMP            (1 << 1)

typedef struct bif_node_t {
  char fname[300]; /* TODO should be dynamic */

  /* supported common attributes */
  uint8_t bootloader; /* boolean */
  uint32_t load;
  uint32_t offset;

  /* supported zynqmp attributes */
  uint8_t fsbl_config; /* boolean */
  char destination_device[100];
  char destination_cpu[100];
  char exception_level[100];

  /* special, non-bootgen features */
  uint8_t is_file; /* for now equal to !fsbl_config */
} bif_node_t;

typedef struct bif_cfg_t {
  char name[300]; /* TODO Shoud be dynamic */

  uint8_t arch;

  uint16_t nodes_num;
  uint16_t nodes_avail;

  bif_node_t nodes[BIF_MAX_NODES_NUM]; /* TODO make it dynamic */
} bif_cfg_t;

int init_bif_cfg(bif_cfg_t *cfg);
int deinit_bif_cfg(bif_cfg_t *cfg);
int bif_cfg_add_node(bif_cfg_t *cfg, bif_node_t *node);
int bif_node_set_attr(bif_cfg_t *cfg, bif_node_t *node, char *attr_name, char *value);
int parse_bif(const char* fname, bif_cfg_t *cfg);

#endif /* BIF_PARSER_H */
