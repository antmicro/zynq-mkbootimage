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
#include <arch/zynqmp.h>

int zynqmp_bootrom_init_offs(uint32_t *img_ptr, bootrom_offs_t *offs) {
  /* Copy the image pointer */
  offs->img_ptr = img_ptr;

  /* Init constant offsets */
  /* Common offsets */
  offs->img_hdr_off = BOOTROM_IMG_HDR_OFF;
  offs->part_hdr_end_off = BOOTROM_PART_HDR_END_OFF;

  /* Zynqmp specific offsets */
  offs->part_hdr_off = BOOTROM_PART_HDR_OFF_ZMP;
  offs->bins_off = BOOTROM_BINS_OFF_ZMP;

  /* Move the offset to reserve the space for headers */
  offs->poff = (offs->img_hdr_off) / sizeof(uint32_t) + img_ptr;
  offs->coff = (offs->bins_off) / sizeof(uint32_t) + img_ptr;

  return BOOTROM_SUCCESS;
}

int zynqmp_bootrom_init_header(bootrom_hdr_t *hdr, bootrom_offs_t *offs) {
  int i;
  int ret;

  /* Call the common init */
  ret = bootrom_init_header(hdr);
  if (ret != BOOTROM_SUCCESS)
    return ret;

  hdr->fsbl_execution_addr = BOOTROM_FSBL_EXEC_ADDR;

  /* Obfuscated keys not yet supported */
  memset(hdr->obfuscated_key, 0x0, sizeof(hdr->obfuscated_key));

  hdr->reserved_zynqmp = BOOTROM_RESERVED_ZMP_RL;

  memset(hdr->user_defined_zynqmp_0, 0x0, sizeof(hdr->user_defined_zynqmp_0));

  hdr->user_defined_zynqmp_0[10] = offs->img_hdr_off;
  hdr->user_defined_zynqmp_0[11] = offs->part_hdr_off;

  memset(hdr->sec_hdr_init_vec, 0x0, sizeof(hdr->sec_hdr_init_vec));
  memset(hdr->obf_key_init_vec, 0x0, sizeof(hdr->obf_key_init_vec));

  /* Memory acces ranges - set to full (0x0 - 0xFFFFFFFF range) */
  for (i = 0; i < 256; i++) {
    hdr->reg_init_zynqmp[2 * i] = 0xFFFFFFFF;
    hdr->reg_init_zynqmp[(2 * i) + 1] = 0x0;
  }

  /* Fill padding */
  memset(hdr->padding, 0xFFFFFFFF, sizeof(hdr->padding));

  /* Calculate the checksum */
  bootrom_calc_hdr_checksum(hdr);

  return BOOTROM_SUCCESS;
}

int zynqmp_bootrom_setup_fsbl_at_curr_off(bootrom_hdr_t* hdr,
                                          bootrom_offs_t* offs,
                                          uint32_t img_len) {
  /* Update the header to point at the bootloader */
  hdr->src_offset = (offs->coff - offs->img_ptr) * sizeof(uint32_t);

  /* Zynqmp seems to round out the len to 8B */
  while (img_len % 8)
    img_len++;

  /* Image length needs to be in words not bytes */
  hdr->fsbl_img_len = img_len;
  hdr->total_img_len = img_len;

  /* Set target cpu */
  hdr->fsbl_target_cpu = BOOTROM_FSBL_CPU_A53_64;

  /* Recalculate the checksum */
  bootrom_calc_hdr_checksum(hdr);

  return BOOTROM_SUCCESS;
}

int zynqmp_bootrom_init_img_hdr_tab(bootrom_img_hdr_tab_t *img_hdr_tab,
                                    bootrom_img_hdr_t *img_hdr,
                                    bootrom_partition_hdr_t *part_hdr,
                                    bootrom_offs_t *offs) {
  /* Call the common code */
  bootrom_init_img_hdr_tab(img_hdr_tab, img_hdr, part_hdr, offs);

  /* Set boot device */
  img_hdr_tab->boot_dev = BOOTROM_IMG_HDR_BOOT_SAME;

  /* Fill reserved fields with zeroes  */
  memset(img_hdr_tab->reserved, 0x0, sizeof(img_hdr_tab->reserved));

  /* Recalculate the checksum */
  img_hdr_tab->checksum = calc_checksum((uint32_t*)&img_hdr_tab,
                                        &(img_hdr_tab->checksum) - 1);

  return BOOTROM_SUCCESS;
}

/* Define ops */
bootrom_ops_t zynqmp_bops = {
  .init_offs = zynqmp_bootrom_init_offs,
  .init_header = zynqmp_bootrom_init_header,
  .setup_fsbl_at_curr_off = zynqmp_bootrom_setup_fsbl_at_curr_off,
  .init_img_hdr_tab = zynqmp_bootrom_init_img_hdr_tab
};
