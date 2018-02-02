#ifndef ARCH_ZYNQMP_H
#define ARCH_ZYNQMP_H

#include <stdint.h>
#include "bootrom.h"

bootrom_ops_t zynqmp_bops;

typedef struct bootrom_partition_hdr_zynqmp_t {
  uint32_t pd_len;
  uint32_t ed_len;
  uint32_t total_len;

  uint32_t next_part_hdr_off;
  uint32_t dest_exec_addr_lo;
  uint32_t dest_exec_addr_hi;
  uint32_t dest_load_addr_lo;
  uint32_t dest_load_addr_hi;
  uint32_t actual_part_off;
  uint32_t attributes;
  union {
    uint32_t reserved0;
    uint32_t section_count;
  };
  uint32_t checksum_off;
  union {
    uint32_t img_hdr_off;
    uint32_t reserved1;
  };
  union {
    uint32_t reserved2;
    uint32_t cert_off;
  };
  uint32_t reserved3;
  uint32_t checksum;
} bootrom_partition_hdr_zynqmp_t;

#endif
