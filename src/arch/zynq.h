#ifndef ARCH_ZYNQ_H
#define ARCH_ZYNQ_H

#include <stdint.h>
#include "bootrom.h"

bootrom_ops_t zynq_bops;

typedef struct bootrom_partition_hdr_zynq_t {
  uint32_t pd_len; /* encrypted partiton data length */
  uint32_t ed_len; /* unecrypted data length */
  uint32_t total_len; /* total encrypted, padding,expansion, auth length */

  uint32_t dest_load_addr; /* RAM addr where the part will be loaded */
  uint32_t dest_exec_addr;
  uint32_t data_off;
  uint32_t attributes;
  uint32_t section_count;

  uint32_t checksum_off;
  uint32_t img_hdr_off;
  uint32_t cert_off;

  uint32_t reserved[4]; /* set to 0 */
  uint32_t checksum;
} bootrom_partition_hdr_zynq_t;

#endif
