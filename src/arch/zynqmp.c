#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>

#include <bif.h>
#include <bootrom.h>

#include <common.h>
#include <arch/common.h>
#include <arch/zynqmp.h>

#define BOOTROM_ZYNQMP_OFFSET_AFTER_HEADERS 0x40

int zynqmp_bootrom_init_offs(uint32_t *img_ptr, int hdr_count, bootrom_offs_t *offs) {
  /* Copy the image pointer */
  offs->img_ptr = img_ptr;

  /* Init constant offsets */
  offs->img_hdr_off = BOOTROM_IMG_HDR_OFF;
  offs->part_hdr_end_off = 0; /* Not needed by zynqmp */
  offs->part_hdr_off = offs->img_hdr_off + sizeof(bootrom_img_hdr_tab_t)
                     + sizeof(bootrom_img_hdr_t) * hdr_count;
  offs->bins_off = offs->part_hdr_off
                 + sizeof(bootrom_partition_hdr_t) * hdr_count
                 + BOOTROM_ZYNQMP_OFFSET_AFTER_HEADERS;

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
                                    bootrom_partition_hdr_t *ihdr,
                                    bootrom_offs_t *offs) {
  unsigned int i;
  uint32_t img_hdr_size = 0;

  bootrom_partition_hdr_zynqmp_t *part_hdr;
  part_hdr = (bootrom_partition_hdr_zynqmp_t*) ihdr;

  /* Call the common code */
  bootrom_init_img_hdr_tab(img_hdr_tab, offs);

  for (i = 0; i < img_hdr_tab->hdrs_count; i++) {
    img_hdr_size = sizeof(img_hdr[i]) / sizeof(uint32_t);
    memset(&img_hdr[i].padding, 0xFF, sizeof(img_hdr->padding));

    img_hdr[i].part_hdr_off =
      (offs->part_hdr_off / sizeof(uint32_t)) +
      (i * sizeof(bootrom_partition_hdr_t) / sizeof(uint32_t));

    /* Calculate the next img hdr offsets */
    if (i + 1 == img_hdr_tab->hdrs_count) {
      img_hdr[i].next_img_off = 0x0;
    } else {
      img_hdr[i].next_img_off = offs->poff + img_hdr_size - offs->img_ptr;
    }

    part_hdr[i].next_part_hdr_off = 0x0;
    if (i > 0) {
      part_hdr[i - 1].next_part_hdr_off = img_hdr[i].part_hdr_off;
      part_hdr[i - 1].checksum =
        calc_checksum(&(part_hdr[i - 1].pd_len),
                      &(part_hdr[i - 1].checksum) - 1);
    }

    /* Write the actual img_hdr data */
    memcpy(offs->poff, &(img_hdr[i]), sizeof(img_hdr[i]));

    /* Keep the offset for later use */
    part_hdr[i].img_hdr_off = (offs->poff - offs->img_ptr);

    /* Calculate the checksum if this is the last header*/
    if (i + 1 == img_hdr_tab->hdrs_count) {
      part_hdr[i].checksum = calc_checksum(&(part_hdr[i].pd_len),
                                           &(part_hdr[i].checksum) - 1);
    }

    if (i == 0) {
      img_hdr_tab->part_img_hdr_off = (offs->poff - offs->img_ptr);
    }

    offs->poff += img_hdr_size;
  }

  /* Fill the partition header offset in img header */
  img_hdr_tab->part_hdr_off = offs->part_hdr_off / sizeof(uint32_t);

  /* Set boot device */
  img_hdr_tab->boot_dev = BOOTROM_IMG_HDR_BOOT_SAME;

  /* Fill reserved fields with zeroes  */
  memset(img_hdr_tab->reserved, 0x0, sizeof(img_hdr_tab->reserved));

  /* Recalculate the checksum */
  img_hdr_tab->checksum = calc_checksum(&img_hdr_tab->version,
                                        &(img_hdr_tab->checksum) - 1);

  return BOOTROM_SUCCESS;
}

uint32_t zynqmp_calc_part_hdr_attr(bif_node_t *node) {
  uint32_t attr = 0;

  if (node->partition_owner == OWNER_FSBL)
    attr |= BOOTROM_PART_ATTR_OWNER_FSBL;
  else if (node->partition_owner == OWNER_UBOOT)
    attr |= BOOTROM_PART_ATTR_OWNER_UBOOT;

  if (node->destination_device == DST_DEV_PS)
    attr |= BOOTROM_PART_ATTR_DEST_DEV_PS;
  else if (node->destination_device == DST_DEV_PL)
    attr |= BOOTROM_PART_ATTR_DEST_DEV_PL;

  switch (node->destination_cpu) {
    case DST_CPU_A53_0:
      attr |= BOOTROM_PART_ATTR_DEST_CPU_A53_0;
      break;
    case DST_CPU_A53_1:
      attr |= BOOTROM_PART_ATTR_DEST_CPU_A53_1;
      break;
    case DST_CPU_A53_2:
      attr |= BOOTROM_PART_ATTR_DEST_CPU_A53_2;
      break;
    case DST_CPU_A53_3:
      attr |= BOOTROM_PART_ATTR_DEST_CPU_A53_3;
      break;
    case DST_CPU_R5_0:
      attr |= BOOTROM_PART_ATTR_DEST_CPU_R5_0;
      break;
    case DST_CPU_R5_1:
      attr |= BOOTROM_PART_ATTR_DEST_CPU_R5_1;
      break;
    case DST_CPU_R5_LOCKSTEP:
      attr |= BOOTROM_PART_ATTR_DEST_CPU_R5_L;
      break;
    default:
      break;
  }

  switch (node->exception_level) {
    case EL_0:
      attr |= BOOTROM_PART_ATTR_EXC_LVL_EL0;
      break;
    case EL_1:
      attr |= BOOTROM_PART_ATTR_EXC_LVL_EL1;
      break;
    case EL_2:
      attr |= BOOTROM_PART_ATTR_EXC_LVL_EL2;
      break;
    case EL_3:
      attr |= BOOTROM_PART_ATTR_EXC_LVL_EL3;
      break;
    default:
      break;
  }

  return attr;
}

int zynqmp_init_part_hdr_default(bootrom_partition_hdr_t *ihdr,
                                 bif_node_t *node) {
  /* Retrieve the header */
  bootrom_partition_hdr_zynqmp_t *hdr;
  hdr = (bootrom_partition_hdr_zynqmp_t*) ihdr;

  /* set destination device as the only attribute */
  hdr->attributes = zynqmp_calc_part_hdr_attr(node);

  hdr->dest_load_addr_lo = node->load;
  hdr->dest_exec_addr_hi = 0x0;
  return BOOTROM_SUCCESS;
}

int zynqmp_init_part_hdr_elf(bootrom_partition_hdr_t *ihdr,
                             bif_node_t *node,
                             uint32_t *size,
                             uint32_t load,
                             uint32_t entry,
                             uint8_t nbits) {
  /* Retrieve the header */
  bootrom_partition_hdr_zynqmp_t *hdr;
  hdr = (bootrom_partition_hdr_zynqmp_t*) ihdr;

  /* Set the load and execution address */
  hdr->dest_load_addr_lo = load;
  hdr->dest_exec_addr_lo = entry;

  /* Size needs to be rounded after conversion to words  */
  *size = (*size + 3) & ~3u;

  /* Set sizes (in words) */
  hdr->pd_len = *size / 4;
  hdr->ed_len = *size / 4;
  hdr->total_len = *size / 4;

  /* Set destination device as the only attribute */
  hdr->attributes = zynqmp_calc_part_hdr_attr(node);

  /* Set the execution state arguments */
  if (nbits == 32) {
    hdr->attributes |= BOOTROM_PART_ATTR_A5X_EXEC_S_32;
    /* Additionally, for some reason, 32bit elfs seem to always overwrite
     * the exception level to EL2 */
    hdr->attributes &= (~BOOTROM_PART_ATTR_EXC_LVL_MASK);
    hdr->attributes |= BOOTROM_PART_ATTR_EXC_LVL_EL2;
  } else if (nbits == 64) {
    hdr->attributes |= BOOTROM_PART_ATTR_A5X_EXEC_S_64;
  }

  return BOOTROM_SUCCESS;
}

int zynqmp_init_part_hdr_bitstream(bootrom_partition_hdr_t *ihdr,
                                   bif_node_t *node) {
  (void) node;
  /* Retrieve the header */
  bootrom_partition_hdr_zynqmp_t *hdr;
  hdr = (bootrom_partition_hdr_zynqmp_t*) ihdr;

  /* Set destination device as the only attribute */
  hdr->attributes = zynqmp_calc_part_hdr_attr(node);

  /* No execution address for bitstream */
  hdr->dest_load_addr_lo = 0xffffffff; /* Don't ask me why  */
  hdr->dest_load_addr_hi = 0x0;
  hdr->dest_exec_addr_lo = 0x0;
  hdr->dest_exec_addr_hi = 0x0;
  return BOOTROM_SUCCESS;
}

int zynqmp_init_part_hdr_linux(bootrom_partition_hdr_t *ihdr,
                               bif_node_t *node,
                               linux_image_header_t *img) {
  /* Handle unused parameters warning */
  (void) node;

  /* Retrieve the header */
  bootrom_partition_hdr_zynqmp_t *hdr;
  hdr = (bootrom_partition_hdr_zynqmp_t*) ihdr;

  hdr->attributes = zynqmp_calc_part_hdr_attr(node);

  if (img->type == FILE_LINUX_IMG_TYPE_UIM) {
    hdr->attributes = BINARY_ATTR_LINUX;
  }

  /* TODO implement and test me */

  return BOOTROM_SUCCESS;
}

int zynqmp_finish_part_hdr(bootrom_partition_hdr_t *ihdr,
                           uint32_t *img_size,
                           bootrom_offs_t *offs) {
  /* Retrieve the header */
  bootrom_partition_hdr_zynqmp_t *hdr;
  hdr = (bootrom_partition_hdr_zynqmp_t*) ihdr;

  /* Set lengths to basic img_len if not set earlier */
  if (hdr->pd_len == 0)
    hdr->pd_len = *img_size;

  if (hdr->ed_len == 0)
    hdr->ed_len = *img_size;

  if (hdr->total_len == 0)
    hdr->total_len = *img_size;

  /* Fill remaining fields that don't seem to be used */
  hdr->checksum_off = 0x0;

  /* Section count is always set to 1 */
  hdr->section_count = 0x1;

  hdr->next_part_hdr_off = 0x0;
  hdr->actual_part_off = (offs->coff - offs->img_ptr);

  /* Add 0xFF padding */
  while (*img_size % (BOOTROM_IMG_PADDING_SIZE / sizeof(uint32_t))) {
    memset(offs->coff + (*img_size), 0xFF, sizeof(uint32_t));
    (*img_size)++;
  }

  return BOOTROM_SUCCESS;
}

/* Define ops */
bootrom_ops_t zynqmp_bops = {
  .init_offs = zynqmp_bootrom_init_offs,
  .init_header = zynqmp_bootrom_init_header,
  .setup_fsbl_at_curr_off = zynqmp_bootrom_setup_fsbl_at_curr_off,
  .init_img_hdr_tab = zynqmp_bootrom_init_img_hdr_tab,
  .init_part_hdr_default = zynqmp_init_part_hdr_default,
  .init_part_hdr_dtb = zynqmp_init_part_hdr_default, /* XXX unsupported yet */
  .init_part_hdr_elf = zynqmp_init_part_hdr_elf,
  .init_part_hdr_bitstream = zynqmp_init_part_hdr_bitstream,
  .init_part_hdr_linux = zynqmp_init_part_hdr_linux,
  .finish_part_hdr = zynqmp_finish_part_hdr,
  .append_null_part = 1 /* yes */
};
