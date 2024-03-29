/* Copyright (c) 2013-2021, Antmicro Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bif.h>
#include <bootrom.h>
#include <common.h>
#include <ctype.h>
#include <errno.h>

static int perrorf(lexer_t *lex, const char *fmt, ...);

static inline char *get_token_name(int type);

static inline void update_pos(lexer_t *lex, char ch);
static inline error append_token(lexer_t *lex, char ch);

static error bif_scan(lexer_t *lex);
static inline error bif_consume(lexer_t *lex, int type);
static inline error bif_expect(lexer_t *lex, int type);
static error bif_parse_file(lexer_t *lex, bif_cfg_t *cfg, bif_node_t *node);
static error bif_parse_attribute(lexer_t *lex, bif_cfg_t *cfg, bif_node_t *node);

static const char *special_chars = ":{}[],=\\";

/* errorf equivalent for parser errors */
static int perrorf(lexer_t *lex, const char *fmt, ...) {
  int n;
  va_list args;

  va_start(args, fmt);
  fprintf(stderr, "error: %s:%d:%d: ", lex->fname, lex->line, lex->column);
  n = vfprintf(stderr, fmt, args);
  va_end(args);

  return n;
}

error init_bif_cfg(bif_cfg_t *cfg) {
  /* Initially setup 8 nodes */
  cfg->nodes_num = 0;
  cfg->nodes_avail = 8;

  /* Alloc memory for it */
  cfg->nodes = malloc(sizeof(bif_node_t) * cfg->nodes_avail);
  if (!cfg->nodes) {
    return ERROR_NOMEM;
  }

  return SUCCESS;
}

error deinit_bif_cfg(bif_cfg_t *cfg) {
  cfg->nodes_num = 0;
  cfg->nodes_avail = 0;

  free(cfg->nodes);

  return SUCCESS;
}

static error init_lexer(lexer_t *lex, const char *fname) {
  error err;

  if (!(lex->file = fopen(fname, "r"))) {
    errorf("could not open file \"%s\"\n", fname);
    return ERROR_BIF_NOFILE;
  }

  lex->fname = malloc(strlen(fname) + 1);
  strcpy(lex->fname, fname);

  lex->line = lex->column = 1;
  lex->type = 0;

  lex->len = lex->cap = 32;
  lex->buffer = malloc(lex->cap * sizeof(char));

  /* Scan a first token
     as the lexer is assumed to always contain a next token
     information in the buffer and type attributes */
  if ((err = bif_scan(lex)))
    return err;

  return SUCCESS;
}

static error deinit_lexer(lexer_t *lex) {
  fclose(lex->file);
  free(lex->fname);
  free(lex->buffer);

  return SUCCESS;
}

static inline char *get_token_name(int type) {
  switch (type) {
  case ':':
    return "':' operator";
  case '{':
    return "'{' operator";
  case '}':
    return "'}' operator";
  case '[':
    return "'[' operator";
  case ']':
    return "']' operator";
  case ',':
    return "',' operator";
  case '=':
    return "'=' operator";
  case '/':
    return "'/' operator";
  case '\\':
    return "'\\' operator";

  case TOKEN_EOF:
    return "end of file";
  case TOKEN_NAME:
    return "a string";
  }
  return "unknown token";
}

static inline void update_pos(lexer_t *lex, char ch) {
  /* Calculate the lexer position after reading ch */

  if (ch == '\t') {
    lex->column += 8;
  } else if (ch == '\n') {
    lex->column = 1;
    lex->line++;
  } else {
    lex->column++;
  }
}

static inline error append_token(lexer_t *lex, char ch) {
  lex->len++;

  if (lex->len >= lex->cap) {
    lex->cap = 2 * (lex->len);
    lex->buffer = realloc(lex->buffer, lex->cap);
    if (!lex->buffer) {
      errorf("out of memory\n");
      return ERROR_NOMEM;
    }
  }

  lex->buffer[lex->len - 1] = ch;
  lex->buffer[lex->len] = '\0';
  return SUCCESS;
}

static inline error bif_scan_comment(lexer_t *lex, char type) {
  int ch = 0, prev;

  if (type == '*') {
    for (;;) {
      prev = ch;
      /* Break if its the end of file */
      if ((ch = fgetc(lex->file)) < 0) {
        perrorf(lex, "file ended while scanning a C-style comment\n");
        return ERROR_BIF_LEXER;
      }
      /* Break if its the end of the comment */
      if (prev == '*' && ch == '/')
        break;
      update_pos(lex, ch);
    }
    /* Don't update the pos after the last ch, it will happen in bif_scan */
  } else if (type == '/') {
    while ((ch = fgetc(lex->file)) >= 0 && ch != '\n')
      update_pos(lex, ch);
    /* Don't update the pos after the last ch, it will happen in bif_scan */
  }
  return SUCCESS;
}

static error bif_scan(lexer_t *lex) {
  /* Scan a single token from a BIF file */

  int ch = 0, prev, err;
  bool esc = false;

  /* Skip white chars and comments */
  for (;;) {
    prev = ch;
    ch = fgetc(lex->file);

    if (ch < 0) {
      lex->type = TOKEN_EOF;
      return SUCCESS;
    } else if (prev == '/') {
      if (ch == '/' || ch == '*') {
        /* Update the pos before reading the comment */
        update_pos(lex, ch);

        if ((err = bif_scan_comment(lex, ch)))
          return err;

        /* Replace ch with a more neutral char than '/' and '*' */
        ch = ch == '/' ? '\n' : ' ';
      } else {
        /* The char after initial '/' didn't start a comment,
           so pretend it wasn't read */
        ungetc(ch, lex->file);
        ch = prev;
        break;
      }
    } else if (prev == '*' && ch == '/') {
      perrorf(lex, "comment end without start\n");
      return ERROR_BIF_LEXER;
    } else if (ch == '/' || ch == '*') {
      /* Ignore a single '/' as it might start a comment
         '*' might end a comment that didn't start */
      ;
    } else if (!isspace(ch)) {
      break;
    }

    update_pos(lex, ch);
  }

  lex->len = 0;

  update_pos(lex, ch);
  if ((err = append_token(lex, ch)))
    return err;

  if (strchr(special_chars, ch)) {
    /* Parse a special character */
    lex->type = ch; /* This is why ASCII is skipped in the enum */
  } else if (ch == '\"') {
    /* Scan a string delimited by " */

    /* Ignore the leading quotation mark */
    lex->len = 0;

    for (;;) {
      ch = fgetc(lex->file);

      if (ch < 0) {
        perrorf(lex, "file ended while scanning a string\n");
        return ERROR_BIF_LEXER;
      } else if (ch == '\\') {
        esc = true;
        continue;
      } else if (ch == '\"' && !esc) {
        lex->type = TOKEN_NAME;
        break;
      } else if (esc) {
        perrorf(lex, "only escape for '\"' char is supported\n");
      }

      update_pos(lex, ch);
      if ((err = append_token(lex, ch)))
        return err;
    }
  } else {
    /* Scan a string being defined without " marks */
    for (;;) {
      ch = fgetc(lex->file);

      if (ch < 0 || strchr(special_chars, ch) || isspace(ch)) {
        ungetc(ch, lex->file); /* we don't want that char */
        lex->type = TOKEN_NAME;
        break;
      }

      update_pos(lex, ch);
      if ((err = append_token(lex, ch)))
        return err;
    }
  }

  return SUCCESS;
}

static inline error bif_consume(lexer_t *lex, int type) {
  /* Read next token if its of the assumed type */
  error err;

  if (lex->type != type)
    return ERROR_BIF_PARSER;
  if ((err = bif_scan(lex)))
    return err;
  return SUCCESS;
}

static inline error bif_expect(lexer_t *lex, int type) {
  /* Throw errors if the next token is unexpected */
  error err = bif_consume(lex, type);

  if (err == ERROR_BIF_PARSER)
    perrorf(lex, "expected %s, got %s\n", get_token_name(type), get_token_name(lex->type));
  return err;
}

static error bif_parse_file(lexer_t *lex, bif_cfg_t *cfg, bif_node_t *node) {
  error err;

  node->load = 0;
  node->offset = 0;
  node->bootloader = 0;
  node->fsbl_config = 0;
  node->pmufw_image = 0;
  node->exception_level = BOOTROM_PART_ATTR_EXC_LVL_EL0;
  node->partition_owner = BOOTROM_PART_ATTR_OWNER_FSBL;
  node->destination_cpu = BOOTROM_PART_ATTR_DEST_CPU_NONE;
  node->destination_device = BOOTROM_PART_ATTR_DEST_DEV_NONE;
  node->is_file = 1;

  /* Parse the attribute list if it's present */
  if (!bif_consume(lex, '[')) {
    /* If the bracket was opened, at least one attribute must be present */

    /* Ignore the return value as the initial comma is optional */
    bif_consume(lex, ',');

    do {
      if ((err = bif_parse_attribute(lex, cfg, node)))
        return err;
    } while (!bif_consume(lex, ','));

    if ((err = bif_expect(lex, ']')))
      return err;
  }

  /* Expect a filename */
  if (lex->type != TOKEN_NAME)
    return bif_expect(lex, TOKEN_NAME);
  strcpy(node->fname, lex->buffer);
  if ((err = bif_consume(lex, TOKEN_NAME)))
    return err;

  return SUCCESS;
}

static error bif_parse_attribute(lexer_t *lex, bif_cfg_t *cfg, bif_node_t *node) {
  error err;
  char *key = NULL, *value = NULL;

  /* Parse an attribute name */
  if (lex->type != TOKEN_NAME)
    return bif_expect(lex, TOKEN_NAME);
  key = malloc(lex->len);
  strcpy(key, lex->buffer);
  if ((err = bif_consume(lex, TOKEN_NAME)))
    return err;

  /* Parse the attribute value if it was present */
  if (!bif_consume(lex, '=')) {
    if (lex->type != TOKEN_NAME)
      return bif_expect(lex, TOKEN_NAME);
    value = malloc(lex->len);
    strcpy(value, lex->buffer);
    if ((err = bif_consume(lex, TOKEN_NAME)))
      return err;
  }

  /* If the value wasn't present, it stays NULL */
  err = bif_node_set_attr(lex, cfg, node, key, value);

  free(key);
  free(value);

  return err;
}

error bif_parse(const char *fname, bif_cfg_t *cfg) {
  error err;
  lexer_t lex;
  bif_node_t node;

  /* Initialize the lexer */
  if ((err = init_lexer(&lex, fname)))
    return err;

  /* First parse the name */
  if ((err = bif_expect(&lex, TOKEN_NAME)))
    return err;
  if ((err = bif_expect(&lex, ':')))
    return err;
  if ((err = bif_expect(&lex, '{')))
    return err;
  /* Parse the file list */
  do {
    if ((err = bif_parse_file(&lex, cfg, &node)))
      return err;
    if ((err = bif_cfg_add_node(cfg, &node)))
      return err;
  } while (lex.type == TOKEN_NAME || lex.type == '[');
  if ((err = bif_expect(&lex, '}')))
    return err;

  deinit_lexer(&lex);

  return SUCCESS;
}

error bif_node_set_attr(
  lexer_t *lex, bif_cfg_t *cfg, bif_node_t *node, char *attr_name, char *value) {
  uint32_t mask;
  /* TODO: parser errors on wrong scans */

  if (strcmp(attr_name, "bootloader") == 0) {
    node->bootloader = 0xFF;
    return SUCCESS;
  }

  if (strcmp(attr_name, "load") == 0) {
    if (!value) {
      perrorf(lex, "the \"%s\" attribute requires an argument\n", attr_name);
      return ERROR_BIF_PARSER;
    }
    if (sscanf(value, "0x%08x", &(node->load)) < 1) {
      perrorf(lex, "the value \"%s\" in an improper format, expected '0xhhhhhhhh' form\n", value);
      return ERROR_BIF_PARSER;
    }
    return SUCCESS;
  }

  if (strcmp(attr_name, "offset") == 0) {
    if (!value) {
      perrorf(lex, "the \"%s\" attribute requires an argument\n", attr_name);
      return ERROR_BIF_PARSER;
    }
    if (sscanf(value, "0x%08x", &(node->offset)) < 1) {
      perrorf(lex, "the value \"%s\" in an improper format, expected '0xhhhhhhhh' form\n", value);
      return ERROR_BIF_PARSER;
    }
    return SUCCESS;
  }

  if (strcmp(attr_name, "partition_owner") == 0) {
    if (!value) {
      perrorf(lex, "the \"%s\" attribute requires an argument\n", attr_name);
      return ERROR_BIF_PARSER;
    }
    mask = map_name_to_mask(bootrom_part_attr_owner_names, value);
    if (mask == NOMASK) {
      perrorf(lex, "value: \"%s\" not supported for the \"%s\" attribute\n", value, attr_name);
      return ERROR_BIF_UNSUPPORTED_VAL;
    }
    return SUCCESS;
  }

  /* Only handle these for zynqmp arch */
  if (cfg->arch & BIF_ARCH_ZYNQMP) {
    if (strcmp(attr_name, "fsbl_config") == 0) {
      node->fsbl_config = 0xFF;

      /* This attribute does not refer to file */
      node->is_file = 0x00;
      return SUCCESS;
    }

    if (strcmp(attr_name, "pmufw_image") == 0) {
      node->pmufw_image = 0xFF;
      return SUCCESS;
    }

    if (strcmp(attr_name, "destination_device") == 0) {
      if (!value) {
        perrorf(lex, "the \"%s\" attribute requires an argument\n", attr_name);
        return ERROR_BIF_PARSER;
      }
      mask = map_name_to_mask(bootrom_part_attr_dest_dev_names, value);
      if (mask == NOMASK) {
        perrorf(lex, "value: \"%s\" not supported for the \"%s\" attribute\n", value, attr_name);
        return ERROR_BIF_UNSUPPORTED_VAL;
      }
      node->destination_device = mask;
      return SUCCESS;
    }

    if (strcmp(attr_name, "destination_cpu") == 0) {
      if (!value) {
        perrorf(lex, "the \"%s\" attribute requires an argument\n", attr_name);
        return ERROR_BIF_PARSER;
      }
      mask = map_name_to_mask(bootrom_part_attr_dest_cpu_names, value);
      if (mask == NOMASK) {
        perrorf(lex, "value: \"%s\" not supported for the \"%s\" attribute\n", value, attr_name);
        return ERROR_BIF_UNSUPPORTED_VAL;
      }
      node->destination_cpu = mask;
      return SUCCESS;
    }

    if (strcmp(attr_name, "exception_level") == 0) {
      if (!value) {
        perrorf(lex, "the \"%s\" attribute requires an argument\n", attr_name);
        return ERROR_BIF_PARSER;
      }
      mask = map_name_to_mask(bootrom_part_attr_exc_lvl_names, value);
      if (mask == NOMASK) {
        perrorf(lex, "value: \"%s\" not supported for the \"%s\" attribute\n", value, attr_name);
        return ERROR_BIF_UNSUPPORTED_VAL;
      }
      node->exception_level = mask;
      return SUCCESS;
    }
  }

  perrorf(lex, "node attribute not supported: \"%s\"\n", attr_name);
  return ERROR_BIF_UNSUPPORTED_ATTR;
}

error bif_cfg_add_node(bif_cfg_t *cfg, bif_node_t *node) {
  uint16_t pos;
  bif_node_t tmp_node;

  /* Check if initialized */
  if (cfg->nodes_avail == 0) {
    errorf("the BIF config is not initialized\n");
    return ERROR_BIF_UNINITIALIZED;
  }

  pos = cfg->nodes_num;
  cfg->nodes[pos] = *node;

  /* Move nodes without offset to the top */
  while (pos >= 1 && cfg->nodes[pos - 1].offset && !cfg->nodes[pos].offset) {
    tmp_node = cfg->nodes[pos - 1];
    cfg->nodes[pos - 1] = cfg->nodes[pos];
    cfg->nodes[pos] = tmp_node;
    pos--;
  }

  pos = cfg->nodes_num;

  /* Move special nodes to the top as well */
  while (pos && (cfg->nodes[pos].fsbl_config || cfg->nodes[pos].pmufw_image)) {
    tmp_node = cfg->nodes[pos - 1];
    cfg->nodes[pos - 1] = cfg->nodes[pos];
    cfg->nodes[pos] = tmp_node;
    pos--;
  }

  pos = cfg->nodes_num;

  /* Sort nodes via offset */
  while (pos >= 1 && cfg->nodes[pos - 1].offset > cfg->nodes[pos].offset) {
    tmp_node = cfg->nodes[pos - 1];
    cfg->nodes[pos - 1] = cfg->nodes[pos];
    cfg->nodes[pos] = tmp_node;
    pos--;
  }

  (cfg->nodes_num)++;

  /* Allocate more space if needed */
  if (cfg->nodes_num >= cfg->nodes_avail) {
    cfg->nodes_avail *= 2;
    cfg->nodes = realloc(cfg->nodes, sizeof(bif_node_t) * cfg->nodes_avail);
    if (!cfg->nodes) {
      return ERROR_NOMEM;
    }
  }
  return SUCCESS;
}
