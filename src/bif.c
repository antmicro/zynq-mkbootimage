/* Copyright (c) 2013-2015, Antmicro Ltd
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pcre.h>
#include <errno.h>

#include <bif.h>

int init_bif_cfg(bif_cfg_t *cfg) {
  /* Initially setup 8 nodes */
  cfg->nodes_num = 0;
  cfg->nodes_avail = 8;

  /* Alloc memory for it */
  cfg->nodes = malloc(sizeof (bif_node_t) * cfg->nodes_avail);
  if (!cfg->nodes) {
    return -ENOMEM;
  }

  return BIF_SUCCESS;
}

int deinit_bif_cfg(bif_cfg_t *cfg) {
  cfg->nodes_num = 0;
  cfg->nodes_avail = 0;

  free(cfg->nodes);

  return BIF_SUCCESS;
}

int parse_bif(const char* fname, bif_cfg_t *cfg) {
  FILE *bif_file;
  int bif_size;
  char *bif_content;
  pcre *re;

  if (!(bif_file = fopen(fname, "r"))) {
    return BIF_ERROR_NOFILE;
  }

  /* Find file size */
  fseek(bif_file, 0, SEEK_END);
  bif_size = ftell(bif_file);
  fseek(bif_file, 0, SEEK_SET);

  /* allocate memory and read the whole file */
  bif_content = malloc(bif_size + 1);
  if (!bif_content) {
    return -ENOMEM;
  }
  fread(bif_content, bif_size, 1, bif_file);

  /* Find the beginning and the end */
  char *beg;
  char *end;

  beg = strchr(bif_content, '{');
  end = strchr(bif_content, '}');

  /* Check if correct */
  if (end == 0 || beg == 0 || beg >= end) {
    return BIF_ERROR_PARSER;
  }

  /* extract the actual config */
  char *bif_cfg = malloc(sizeof *bif_cfg * (end-beg));
  if (!bif_cfg) {
    free(bif_content);
    return -ENOMEM;
  }
  memcpy(bif_cfg, beg+1, end-beg-1);
  bif_cfg[end - beg - 1] = '\0';

  /* First extract the name and the parameter group if exists */
  char *pcre_regex = "\\s*(\\[(.*)\\])?\\s*([-/ a-zA-Z._0-9]+)";
  const char *pcre_err;
  int pcre_err_off;
  re = pcre_compile(pcre_regex, 0, &pcre_err, &pcre_err_off, NULL);

  if (re == NULL) {
    return BIF_ERROR_PARSER;
  }

  /* Attributes regex */
  pcre *re_attr;
  char *pcre_attr_regex = "((\\w+)=([\\w-]+))+";
  re_attr = pcre_compile(pcre_attr_regex, 0, &pcre_err, &pcre_err_off, NULL);

  /* TODO cleanup */
  int ret;
  int attr_ret;
  int ovec[30];
  int soff=0;
  int iovec[30];
  int isoff=0;
  char cattr[500];
  char pattr_n[500];
  char pattr_v[500];

  bif_node_t node;

  do {
    /* Clean up node */
    strcpy(node.fname, "");
    node.offset = 0;
    node.load = 0;
    node.bootloader = 0;
    node.fsbl_config = 0;
    node.partition_owner = OWNER_FSBL;
    node.pmufw_image = 0;
    node.destination_device = DST_DEV_UNDEF;
    node.destination_cpu = DST_CPU_UNDEF;
    node.exception_level = EL_UNDEF;
    node.is_file = 1;

    ret = pcre_exec(re, NULL, bif_cfg, strlen(bif_cfg), soff, 0, ovec, 30);
    if (ret < 4) {
      /* no more nodes */
      break;
    }

    /* parse attributes */
    memcpy(cattr, bif_cfg + ovec[4], ovec[5] - ovec[4]);
    cattr[ovec[5] - ovec[4]] = '\0';

    isoff = 0;
    if (re_attr == NULL) {
      return BIF_ERROR_PARSER;
    }
    int aret = 0;
    do {
      aret = pcre_exec(re_attr, NULL, cattr, strlen(cattr), isoff, 0, iovec, 30);
      if (aret < 1 && isoff == 0 && strlen(cattr) > 0) {
        attr_ret  = bif_node_set_attr(cfg, &node, cattr, NULL);

        if (attr_ret != BIF_SUCCESS)
          return attr_ret;
        break;
      }
      int i;
      for (i = 2; i < aret; i+=3) {
        memcpy(pattr_n, cattr + iovec[i*2], iovec[2*i+1] - iovec[2*i]);
        pattr_n[iovec[2*i+1] - iovec[2*i]] = '\0';

        memcpy(pattr_v, cattr + iovec[(i+1)*2], iovec[2*(i+1)+1] - iovec[2*(i+1)]);
        pattr_v[iovec[2*(i+1)+1] - iovec[2*(i+1)]] = '\0';

        attr_ret = bif_node_set_attr(cfg, &node, pattr_n, pattr_v);

        if (attr_ret != BIF_SUCCESS)
          return attr_ret;
      }
      isoff = iovec[1];
    } while (aret > 3);

    /* set filename of the node */
    if (ovec[7] - ovec[6] < PATH_MAX) {
      memcpy(node.fname, bif_cfg + ovec[6], ovec[7] - ovec[6]);
      node.fname[ovec[7] - ovec[6]] = '\0';
    } else {
      fprintf(stderr,
              "File path too long, maximum supported on your system is %d",
              PATH_MAX);
      return BIF_ERROR_PARSER;
    }

    bif_cfg_add_node(cfg, &node);

    soff = ovec[1];
  } while (ret >3);


  free(bif_cfg);
  free(bif_content);
  fclose(bif_file);

  return BIF_SUCCESS;
}

int bif_node_set_attr(bif_cfg_t *cfg, bif_node_t *node, char *attr_name, char *value) {
  if (strcmp(attr_name, "bootloader") == 0) {
    node->bootloader = 0xFF;
    return BIF_SUCCESS;
  }

  if (strcmp(attr_name, "load") == 0 ) {
    sscanf(value, "0x%08x", &(node->load));
    return BIF_SUCCESS;
  }

  if (strcmp(attr_name, "offset") == 0 ) {
    sscanf(value, "0x%08x", &(node->offset));
    return BIF_SUCCESS;
  }

  if (strcmp(attr_name, "partition_owner") == 0) {
    if (strcmp(value, "fsbl") == 0)
      node->partition_owner = OWNER_FSBL;
    else if (strcmp(value, "uboot") == 0)
      node->partition_owner = OWNER_UBOOT;
    else {
      fprintf(stderr, "Value: \"%s\" not supported for the \"%s\" attribute\n", value, attr_name);
      return BIF_ERROR_UNSUPPORTED_VAL;
    }
    return BIF_SUCCESS;
  }

  /* Only handle these for zynqmp arch */
  if (cfg->arch & BIF_ARCH_ZYNQMP) {
    if (strcmp(attr_name, "fsbl_config") == 0) {
      node->fsbl_config = 0xFF;

      /* This attribute does not refer to file */
      node->is_file = 0x00;
      return BIF_SUCCESS;
    }

    if (strcmp(attr_name, "pmufw_image") == 0) {
      node->pmufw_image = 0xFF;
      return BIF_SUCCESS;
    }

    if (strcmp(attr_name, "destination_device") == 0 ) {
      if (strcmp(value, "ps") == 0)
        node->destination_device = DST_DEV_PS;
      else if (strcmp(value, "pl") == 0)
        node->destination_device = DST_DEV_PL;
      else {
        fprintf(stderr, "Value: \"%s\" not supported for the \"%s\" attribute\n", value, attr_name);
        return BIF_ERROR_UNSUPPORTED_VAL;
      }

      return BIF_SUCCESS;
    }

    if (strcmp(attr_name, "destination_cpu") == 0 ) {
      if (strcmp(value, "a53-0") == 0)
        node->destination_cpu = DST_CPU_A53_0;
      else if (strcmp(value, "a53-1") == 0)
        node->destination_cpu = DST_CPU_A53_1;
      else if (strcmp(value, "a53-2") == 0)
        node->destination_cpu = DST_CPU_A53_2;
      else if (strcmp(value, "a53-3") == 0)
        node->destination_cpu = DST_CPU_A53_3;
      else if (strcmp(value, "r5-0") == 0)
        node->destination_cpu = DST_CPU_R5_0;
      else if (strcmp(value, "r5-1") == 0)
        node->destination_cpu = DST_CPU_R5_1;
      else if (strcmp(value, "r5-lockstep") == 0)
        node->destination_cpu = DST_CPU_R5_LOCKSTEP;
      else {
        fprintf(stderr, "Value: \"%s\" not supported for the \"%s\" attribute\n", value, attr_name);
        return BIF_ERROR_UNSUPPORTED_VAL;
      }
      return BIF_SUCCESS;
    }

    if (strcmp(attr_name, "exception_level") == 0) {
      if (strcmp(value, "el-0") == 0)
        node->exception_level = EL_0;
      else if (strcmp(value, "el-1") == 0)
        node->exception_level = EL_1;
      else if (strcmp(value, "el-2") == 0)
        node->exception_level = EL_2;
      else if (strcmp(value, "el-3") == 0)
        node->exception_level = EL_3;
      else {
        fprintf(stderr, "Value: \"%s\" not supported for the \"%s\" attribute\n", value, attr_name);
        return BIF_ERROR_UNSUPPORTED_VAL;
      }
      return BIF_SUCCESS;
    }
  }

  fprintf(stderr, "Node attribute not supported: \"%s\"\n", attr_name);
  return BIF_ERROR_UNSUPPORTED_ATTR;
}

int bif_cfg_add_node(bif_cfg_t *cfg, bif_node_t *node) {
  uint16_t pos;
  bif_node_t tmp_node;

  /* Check if initialized */
  if (cfg->nodes_avail == 0) {
    return BIF_ERROR_UNINITIALIZED;
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
    cfg->nodes = realloc(cfg->nodes, sizeof (bif_node_t) * cfg->nodes_avail);
    if (!cfg->nodes) {
      return -ENOMEM;
    }
  }
  return BIF_SUCCESS;
}
