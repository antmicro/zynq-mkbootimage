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

  /* Move the offset to reserve the space for headers */
  offs->poff = (BOOTROM_IMG_HDR_OFF) / sizeof(uint32_t) + img_ptr;
  offs->coff = (BOOTROM_BINS_OFF) / sizeof(uint32_t) + img_ptr;

  offs->phoff = BOOTROM_PART_HDR_OFF_ZMP;

  return BOOTROM_SUCCESS;
}

int zynqmp_bootrom_init_header(bootrom_hdr_t *hdr) {
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

  hdr->user_defined_zynqmp_0[10] = BOOTROM_IMG_HDR_OFF;
  hdr->user_defined_zynqmp_0[11] = BOOTROM_PART_HDR_OFF_ZMP;

  memset(hdr->sec_hdr_init_vec, 0x0, sizeof(hdr->sec_hdr_init_vec));
  memset(hdr->obf_key_init_vec, 0x0, sizeof(hdr->obf_key_init_vec));

  /* Memory acces ranges - set to full (0x0 - 0xFFFFFFFF range) */
  for (i = 0; i < 256; i++) {
    hdr->reg_init_zynqmp[2 * i] = 0xFFFFFFFF;
    hdr->reg_init_zynqmp[(2 * i) + 1] = 0x0;
  }

  /* Calculate the checksum */
  bootrom_calc_hdr_checksum(hdr);

  return BOOTROM_SUCCESS;
}

int zynqmp_bootrom_set_target_cpu(bootrom_hdr_t *hdr) {
  /* For now we always set it to arm a54 */
  /* TODO support other CPUs */
  hdr->fsbl_target_cpu = BOOTROM_FSBL_CPU_A53_64;

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
  .set_target_cpu = zynqmp_bootrom_set_target_cpu,
  .init_img_hdr_tab = zynqmp_bootrom_init_img_hdr_tab
};
