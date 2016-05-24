#ifndef BIF_PARSER_H

#define BIF_PARSER_H

#define BIF_MAX_NODES_NUM 10

#define BIF_SUCCESS                0
#define BIF_ERROR_NOFILE           1
#define BIF_ERROR_PARSER           2
#define BIF_ERROR_UNSUPORTED_ATTR  3

typedef struct bif_node_t {
  char fname[300]; /* TODO should be dynamic */

  /* supported attributes */
  uint8_t bootloader; /* boolean */
  uint32_t load;
  uint32_t offset;
} bif_node_t;

typedef struct bif_cfg_t {
  char name[300]; /* TODO Shoud be dynamic */

  uint16_t nodes_num;
  uint16_t nodes_avail;

  bif_node_t nodes[BIF_MAX_NODES_NUM]; /* TODO make it dynamic */
} bif_cfg_t;

int init_bif_cfg(bif_cfg_t *cfg);
int deinit_bif_cfg(bif_cfg_t *cfg);
int bif_cfg_add_node(bif_cfg_t *cfg, bif_node_t *node);
int bif_node_set_attr(bif_node_t *node, char *attr_name, char *value);
int parse_bif(const char* fname, bif_cfg_t *cfg);

#endif /* BIF_PARSER_H */
