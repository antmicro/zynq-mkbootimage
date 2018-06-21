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

int zynq_bootrom_init_offs(uint32_t *img_ptr, int hdr_count, bootrom_offs_t *offs) {
  (void)hdr_count;
  /* Copy the image pointer */
  offs->img_ptr = img_ptr;

  /* Init constant offsets */
  offs->img_hdr_off = BOOTROM_IMG_HDR_OFF;
  offs->part_hdr_off = BOOTROM_PART_HDR_OFF;
  offs->part_hdr_end_off = BOOTROM_PART_HDR_END_PADD;
  offs->bins_off = BOOTROM_BINS_OFF;

  /* Move the offset to reserve the space for headers */
  offs->poff = (offs->img_hdr_off) / sizeof(uint32_t) + img_ptr;
  offs->coff = (offs->bins_off) / sizeof(uint32_t) + img_ptr;

  return BOOTROM_SUCCESS;
}

int zynq_bootrom_init_header(bootrom_hdr_t *hdr, bootrom_offs_t *offs) {
  int ret;
  int i;

  /* Call the common init */
  ret = bootrom_init_header(hdr);
  if (ret != BOOTROM_SUCCESS)
    return ret;

  hdr->user_defined_0 = BOOTROM_USER_0;

  memset(hdr->user_defined_zynq_0, 0x0, sizeof(hdr->user_defined_zynq_0));

  hdr->user_defined_zynq_0[19] = offs->img_hdr_off;
  hdr->user_defined_zynq_0[20] = offs->part_hdr_off;;

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

int zynq_bootrom_setup_fsbl_at_curr_off(bootrom_hdr_t* hdr,
                                        bootrom_offs_t* offs,
                                        uint32_t img_len) {
  /* Update the header to point at the bootloader */
  hdr->src_offset = (offs->coff - offs->img_ptr) * sizeof(uint32_t);

  /* Image length needs to be in words not bytes */
  hdr->img_len = img_len;
  hdr->total_img_len = hdr->img_len;

  /* Recalculate the checksum */
  bootrom_calc_hdr_checksum(hdr);

  return BOOTROM_SUCCESS;
}

int zynq_bootrom_init_img_hdr_tab(bootrom_img_hdr_tab_t *img_hdr_tab,
                                  bootrom_img_hdr_t *img_hdr,
                                  bootrom_partition_hdr_t *ihdr,
                                  bootrom_offs_t *offs) {
  unsigned int i;
  uint32_t img_hdr_size = 0;

  /* Retrieve the header */
  bootrom_partition_hdr_zynq_t *part_hdr;
  part_hdr = (bootrom_partition_hdr_zynq_t*) ihdr;

  /* Call the common code */
  bootrom_init_img_hdr_tab(img_hdr_tab, offs);

  for (i = 0; i < img_hdr_tab->hdrs_count; i++) {
    img_hdr_size = sizeof(img_hdr[i]) / sizeof(uint32_t);
    memset(&img_hdr[i].padding, 0xFF, sizeof(img_hdr->padding));

    /* Calculate the next img hdr offsets */
    if (i + 1 == img_hdr_tab->hdrs_count) {
      img_hdr[i].next_img_off = 0x0;
    } else {
      img_hdr[i].next_img_off = offs->poff + img_hdr_size - offs->img_ptr;
    }

    img_hdr[i].part_hdr_off =
      (offs->part_hdr_off / sizeof(uint32_t)) +
      (i * sizeof(bootrom_partition_hdr_t) / sizeof(uint32_t));

    /* Write the actual img_hdr data */
    memcpy(offs->poff, &(img_hdr[i]), sizeof(img_hdr[i]));

    /* Keep the offset for later use */
    part_hdr[i].img_hdr_off = (offs->poff - offs->img_ptr);

    /* Calculate the checksum */
    part_hdr[i].checksum = calc_checksum(&(part_hdr[i].pd_len),
                                         &(part_hdr[i].checksum) - 1);

    if (i == 0) {
      img_hdr_tab->part_img_hdr_off = (offs->poff - offs->img_ptr);
    }

    offs->poff += img_hdr_size;
  }

  /* Fill the partition header offset in img header */
  img_hdr_tab->part_hdr_off = offs->part_hdr_off / sizeof(uint32_t);

  /* Fill padding */
  memset(img_hdr_tab->padding, 0xFFFFFFFF, sizeof(img_hdr_tab->padding));

  return BOOTROM_SUCCESS;
}

int zynq_init_part_hdr_generic(bootrom_partition_hdr_t *ihdr,
                               bif_node_t *node,
                               uint32_t extra_args) {
  /* Retrieve the header */
  bootrom_partition_hdr_zynq_t *hdr;
  hdr = (bootrom_partition_hdr_zynq_t*) ihdr;

  /* set destination device as the only attribute */
  hdr->attributes =
    (0x1 << BOOTROM_PART_ATTR_DEST_DEV_OFF) | extra_args;

  /* No load/execution address */
  hdr->dest_load_addr = node->load;
  hdr->dest_exec_addr = 0x0;
  return BOOTROM_SUCCESS;
}

int zynq_init_part_hdr_default(bootrom_partition_hdr_t *ihdr,
                               bif_node_t *node) {
  return zynq_init_part_hdr_generic(ihdr, node, BINARY_ATTR_GENERAL);
}

int zynq_init_part_hdr_dtb(bootrom_partition_hdr_t *ihdr,
                           bif_node_t *node) {
  return zynq_init_part_hdr_generic(ihdr, node, BINARY_ATTR_RAMDISK);
}

int zynq_init_part_hdr_elf(bootrom_partition_hdr_t *ihdr,
                           bif_node_t *node,
                           uint32_t *size,
                           uint32_t load,
                           uint32_t entry,
                           uint8_t nbits) {
  /* Handle unused parameters warning */
  (void) node;
  (void) size;
  (void) nbits;

  /* Retrieve the header */
  bootrom_partition_hdr_zynq_t *hdr;
  hdr = (bootrom_partition_hdr_zynq_t*) ihdr;

  /* Set the load and execution address */
  hdr->dest_load_addr = load;
  hdr->dest_exec_addr = entry;

  /* set destination device as the only attribute */
  hdr->attributes = BOOTROM_PART_ATTR_DEST_DEV_PS;

  return BOOTROM_SUCCESS;
}

int zynq_init_part_hdr_bitstream(bootrom_partition_hdr_t *ihdr,
                                 bif_node_t *node) {
  /* Handle unused parameters warning */
  (void) node;

  /* Retrieve the header */
  bootrom_partition_hdr_zynq_t *hdr;
  hdr = (bootrom_partition_hdr_zynq_t*) ihdr;

  /* Set destination device as the only attribute */
  hdr->attributes = BOOTROM_PART_ATTR_DEST_DEV_PL;

  /* No execution address for bitstream */
  hdr->dest_load_addr = 0x0;
  hdr->dest_exec_addr = 0x0;
  return BOOTROM_SUCCESS;
}

int zynq_init_part_hdr_linux(bootrom_partition_hdr_t *ihdr,
                             bif_node_t *node,
                             linux_image_header_t *img) {
  /* Retrieve the header */
  bootrom_partition_hdr_zynq_t *hdr;
  hdr = (bootrom_partition_hdr_zynq_t*) ihdr;
  if (img->type == FILE_LINUX_IMG_TYPE_UIM) {
    hdr->attributes = BINARY_ATTR_LINUX;
  }

  if (img->type == FILE_LINUX_IMG_TYPE_URD)
    hdr->attributes = 0x00; /* despite what the doc says */

  if (img->type == FILE_LINUX_IMG_TYPE_SCR)
    hdr->attributes = BINARY_ATTR_GENERAL;

  /* Set destination device attribute */
  hdr->attributes |= BOOTROM_PART_ATTR_DEST_DEV_PS;

  /* No load/execution address */
  hdr->dest_load_addr = node->load;
  hdr->dest_exec_addr = 0x0;
  return BOOTROM_SUCCESS;
}

int zynq_finish_part_hdr(bootrom_partition_hdr_t *ihdr,
                         uint32_t *img_size,
                         bootrom_offs_t *offs) {
  /* Retrieve the header */
  bootrom_partition_hdr_zynq_t *hdr;
  hdr = (bootrom_partition_hdr_zynq_t*) ihdr;

  /* Is bitstream? */
  if(hdr->attributes == BOOTROM_PART_ATTR_DEST_DEV_PL) {
    /* For some reason, a noop is appended after bitstream (only for zynq) */
    const uint8_t noop[4] = {0, 0, 0, 0x20};
    memcpy(offs->coff + (*img_size), noop, sizeof(noop));
    (*img_size)++;
  }

  hdr->pd_len = *img_size;
  hdr->ed_len = *img_size;
  hdr->total_len = *img_size;

  /* Section count is always set to 1 */
  hdr->section_count = 0x1;

  /* Fill remaining fields that don't seem to be used */
  hdr->checksum_off = 0x0;
  hdr->cert_off = 0x0;
  memset(hdr->reserved, 0x00, sizeof(hdr->reserved));

  /* Fill the offset */
  hdr->data_off = (offs->coff - offs->img_ptr);

  /* Continue adding padding util it hits the correct alignement */
  while (*img_size % (BOOTROM_IMG_PADDING_SIZE / sizeof(uint32_t))) {
    memset(offs->coff + (*img_size), 0xFF, sizeof(uint32_t));
    (*img_size)++;
  }

  return BOOTROM_SUCCESS;
}

/* Define ops */
bootrom_ops_t zynq_bops = {
  .init_offs = zynq_bootrom_init_offs,
  .init_header = zynq_bootrom_init_header,
  .setup_fsbl_at_curr_off = zynq_bootrom_setup_fsbl_at_curr_off,
  .init_img_hdr_tab = zynq_bootrom_init_img_hdr_tab,
  .init_part_hdr_default = zynq_init_part_hdr_default,
  .init_part_hdr_dtb = zynq_init_part_hdr_dtb,
  .init_part_hdr_elf = zynq_init_part_hdr_elf,
  .init_part_hdr_bitstream = zynq_init_part_hdr_bitstream,
  .init_part_hdr_linux = zynq_init_part_hdr_linux,
  .finish_part_hdr = zynq_finish_part_hdr,
  .append_null_part = 0 /* Zynq does not use null part */
};
