#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gelf.h>
#include <unistd.h>
#include <libgen.h>

#include <bif.h>
#include <bootrom.h>

#include <common.h>
#include <arch/common.h>
#include <arch/zynq.h>

int zynq_bootrom_init_offs(uint32_t *img_ptr, bootrom_offs_t *offs) {
  /* Copy the image pointer */
  offs->img_ptr = img_ptr;

  /* Move the offset to reserve the space for headers */
  offs->poff = (BOOTROM_IMG_HDR_OFF) / sizeof(uint32_t) + img_ptr;
  offs->coff = (BOOTROM_BINS_OFF) / sizeof(uint32_t) + img_ptr;

  offs->phoff = BOOTROM_PART_HDR_OFF;

  return BOOTROM_SUCCESS;
}

int zynq_bootrom_init_header(bootrom_hdr_t *hdr) {
  int ret;
  int i;

  /* Call the common init */
  ret = bootrom_init_header(hdr);
  if (ret != BOOTROM_SUCCESS)
    return ret;

  hdr->user_defined_0 = BOOTROM_USER_0;

  memset(hdr->user_defined_zynq_0, 0x0, sizeof(hdr->user_defined_zynq_0));

  hdr->user_defined_zynq_0[17] = 0x0;
  hdr->user_defined_zynq_0[18] = 0x0;
  hdr->user_defined_zynq_0[19] = BOOTROM_IMG_HDR_OFF;
  hdr->user_defined_zynq_0[20] = BOOTROM_PART_HDR_OFF;

  /* Memory acces ranges - set to full (0x0 - 0xFFFFFFFF range) */
  for (i = 0; i < 256; i++) {
    hdr->reg_init_zynq[2 * i] = 0xFFFFFFFF;
    hdr->reg_init_zynq[(2 * i) + 1] = 0x0;
  }

  memset(hdr->user_defined_zynq_1, 0xFF, sizeof(hdr->user_defined_zynq_1));

  /* Calculate the checksum */
  bootrom_calc_hdr_checksum(hdr);

  return BOOTROM_SUCCESS;
}

int zynq_bootrom_set_target_cpu(bootrom_hdr_t *hdr) {
  /* Not required for zynq */
  return BOOTROM_SUCCESS;
}

int zynq_bootrom_init_img_hdr_tab(bootrom_img_hdr_tab_t *img_hdr_tab,
                                  bootrom_img_hdr_t *img_hdr,
                                  bootrom_partition_hdr_t *part_hdr,
                                  bootrom_offs_t *offs) {
  /* Call the common code */
  bootrom_init_img_hdr_tab(img_hdr_tab, img_hdr, part_hdr, offs);

  /* Fill padding */
  memset(img_hdr_tab->padding, 0xFFFFFFFF, sizeof(img_hdr_tab->padding));

  return BOOTROM_SUCCESS;
}

/* Define ops */
bootrom_ops_t zynq_bops = {
  .init_offs = zynq_bootrom_init_offs,
  .init_header = zynq_bootrom_init_header,
  .set_target_cpu = zynq_bootrom_set_target_cpu,
  .init_img_hdr_tab = zynq_bootrom_init_img_hdr_tab
};
