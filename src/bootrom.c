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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arch/common.h>
#include <bif.h>
#include <bootrom.h>
#include <byteswap.h>
#include <common.h>
#include <fcntl.h>
#include <file/bitstream.h>
#include <file/elf.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>

/* clang-format off */
mask_name_t bootrom_part_attr_owner_names[] = {
  {"fsbl",  BOOTROM_PART_ATTR_OWNER_FSBL,  NULL},
  {"uboot", BOOTROM_PART_ATTR_OWNER_UBOOT, NULL},
  {0},
};

mask_name_t bootrom_part_attr_rsa_used_names[] = {
  {"used",     BOOTROM_PART_ATTR_RSA_USED,     NULL},
  {"not used", BOOTROM_PART_ATTR_RSA_NOT_USED, NULL},
  {0},
};

mask_name_t bootrom_part_attr_dest_cpu_names[] = {
  {"none",        BOOTROM_PART_ATTR_DEST_CPU_NONE,  NULL},
  {"a53-0",       BOOTROM_PART_ATTR_DEST_CPU_A53_0, NULL},
  {"a53-1",       BOOTROM_PART_ATTR_DEST_CPU_A53_1, NULL},
  {"a53-2",       BOOTROM_PART_ATTR_DEST_CPU_A53_2, NULL},
  {"a53-3",       BOOTROM_PART_ATTR_DEST_CPU_A53_3, NULL},
  {"r5-0",        BOOTROM_PART_ATTR_DEST_CPU_R5_0,  NULL},
  {"r5-1",        BOOTROM_PART_ATTR_DEST_CPU_R5_1,  NULL},
  {"r5-lockstep", BOOTROM_PART_ATTR_DEST_CPU_R5_L,  NULL},
  {0},
};

mask_name_t bootrom_part_attr_encryption_names[] = {
  {"yes", BOOTROM_PART_ATTR_ENCRYPTION_YES, NULL},
  {"no",  BOOTROM_PART_ATTR_ENCRYPTION_NO,  NULL},
  {0},
};

mask_name_t bootrom_part_attr_dest_dev_names[] = {
  {"none", BOOTROM_PART_ATTR_DEST_DEV_NONE, NULL},
  {"ps",   BOOTROM_PART_ATTR_DEST_DEV_PS,   NULL},
  {"pl",   BOOTROM_PART_ATTR_DEST_DEV_PL,   NULL},
  {"int",  BOOTROM_PART_ATTR_DEST_DEV_INT,  NULL},
  {0},
};

mask_name_t bootrom_part_attr_a5x_exec_s_names[] = {
  {"32-bit", BOOTROM_PART_ATTR_A5X_EXEC_S_32, NULL},
  {"64-bit", BOOTROM_PART_ATTR_A5X_EXEC_S_64, NULL},
  {0},
};

mask_name_t bootrom_part_attr_exc_lvl_names[] = {
  {"el-0", BOOTROM_PART_ATTR_EXC_LVL_EL0, NULL},
  {"el-1", BOOTROM_PART_ATTR_EXC_LVL_EL1, NULL},
  {"el-2", BOOTROM_PART_ATTR_EXC_LVL_EL2, NULL},
  {"el-3", BOOTROM_PART_ATTR_EXC_LVL_EL3, NULL},
  {0},
};

mask_name_t bootrom_part_attr_trust_zone_names[] = {
  {"yes", BOOTROM_PART_ATTR_TRUST_ZONE_YES, NULL},
  {"no",  BOOTROM_PART_ATTR_TRUST_ZONE_NO,  NULL},
  {0},
};

mask_name_t bootrom_part_attr_mask_names[] = {
  {"Owner",               BOOTROM_PART_ATTR_OWNER_MASK,      bootrom_part_attr_owner_names},
  {"RSA",                 BOOTROM_PART_ATTR_RSA_USED_MASK,   bootrom_part_attr_rsa_used_names},
  {"Destination CPU",     BOOTROM_PART_ATTR_DEST_CPU_MASK,   bootrom_part_attr_dest_cpu_names},
  {"Encryption",          BOOTROM_PART_ATTR_ENCRYPTION_MASK, bootrom_part_attr_encryption_names},
  {"Destination Device",  BOOTROM_PART_ATTR_DEST_DEV_MASK,   bootrom_part_attr_dest_dev_names},
  {"A5x Execution State", BOOTROM_PART_ATTR_A5X_EXEC_S_MASK, bootrom_part_attr_a5x_exec_s_names},
  {"Exception Level",     BOOTROM_PART_ATTR_EXC_LVL_MASK,    bootrom_part_attr_exc_lvl_names},
  {"Trust Zone",          BOOTROM_PART_ATTR_TRUST_ZONE_MASK, bootrom_part_attr_trust_zone_names},
  {0}
};
/* clang-format on */

uint32_t map_name_to_mask(mask_name_t mask_names[], char *name) {
  int i;

  for (i = 0; mask_names[i].name; i++)
    if (strcmp(mask_names[i].name, name) == 0)
      return mask_names[i].mask;
  return 0xffffffff;
}

char *map_mask_to_name(mask_name_t mask_names[], uint32_t mask) {
  int i;

  for (i = 0; mask_names[i].name; i++)
    if (mask_names[i].mask == mask)
      return mask_names[i].name;
  return "INVALID";
}

/* Returns the offset by which the addr parameter should be moved
 * and partition header info via argument pointers.
 * The regular return value is the error code. */
error append_file_to_image(uint32_t *addr,
                           bootrom_ops_t *bops,
                           bootrom_offs_t *offs,
                           bif_node_t node,
                           bootrom_partition_hdr_t *part_hdr,
                           uint32_t *img_size) {
  uint32_t file_header;
  struct stat cfile_stat;
  FILE *cfile;
  uint32_t elf_load;
  uint32_t elf_entry;
  uint8_t elf_nbits;
  uint32_t img_size_init;
  linux_image_header_t linux_img;
  error err;

  /* Initialize header with zeroes */
  memset(part_hdr, 0x0, sizeof(*part_hdr));

  img_size_init = *img_size;
  *img_size = 0;

  if (stat(node.fname, &cfile_stat)) {
    errorf("could not stat file: %s\n", node.fname);
    return ERROR_BOOTROM_NOFILE;
  }
  if (!S_ISREG(cfile_stat.st_mode)) {
    errorf("not a regular file: %s\n", node.fname);
    return ERROR_BOOTROM_NOFILE;
  }
  cfile = fopen(node.fname, "rb");

  if (cfile == NULL) {
    errorf("could not open file: %s\n", node.fname);
    return ERROR_BOOTROM_NOFILE;
  }

  /* Check file format */
  fread(&file_header, 1, sizeof(file_header), cfile);

  switch (file_header) {
  case FILE_MAGIC_ELF:
    /* Init elf file (img_size_init is non-zero for a bootloader if there
     * is PMU firmware waiting). File size is used as result size limit
     * as estimate_boot_image_size() makes that same assumption when
     * allocating the memory area for the boot image */
    err = elf_append(addr + img_size_init / sizeof(uint32_t),
                     node.fname,
                     cfile_stat.st_size,
                     img_size,
                     &elf_nbits,
                     &elf_load,
                     &elf_entry);
    if (err) {
      errorf("ELF file reading failed\n");

      /* Close the file */
      fclose(cfile);
      return err;
    }

    /* Init partition header, the img_size_init is non-zero only if this file
     * is a bootloader, and we have the PMU firmware waiting. In this case, the
     * partition header has to take the PMU size into account. That's why the
     * 'init' size is added before initializing the partition header and
     * subtracted after the initialization is done */
    *img_size += img_size_init;
    bops->init_part_hdr_elf(part_hdr, &node, img_size, elf_load, elf_entry, elf_nbits);
    *img_size -= img_size_init;

    break;
  case FILE_MAGIC_XILINXBIT_0:
    /* Verify file */
    if ((err = bitstream_verify(cfile))) {
      errorf("not a valid bitstream file: %s.\n", node.fname);

      /* Close the file */
      fclose(cfile);
      return err;
    }

    /* It matches, append it to the image */
    err = bitstream_append(addr, cfile, img_size);

    if (err)
      return err;

    /* Init partition header */
    bops->init_part_hdr_bitstream(part_hdr, &node);

    break;
  case FILE_MAGIC_LINUX:
    fseek(cfile, 0, SEEK_SET);
    fread(&linux_img, 1, sizeof(linux_img), cfile);

    fseek(cfile, 0, SEEK_SET);
    *img_size = fread(addr, 1, cfile_stat.st_size, cfile);

    /* Init partition header */
    bops->init_part_hdr_linux(part_hdr, &node, &linux_img);

    break;
  case FILE_MAGIC_DTB:
    fseek(cfile, 0, SEEK_SET);
    *img_size = fread(addr, 1, cfile_stat.st_size, cfile);

    bops->init_part_hdr_dtb(part_hdr, &node);
    break;
  default: /* Treat as a binary file */

    fseek(cfile, 0, SEEK_SET);
    *img_size = fread(addr, 1, cfile_stat.st_size, cfile);

    bops->init_part_hdr_default(part_hdr, &node);
  };

  *img_size += img_size_init;
  /* Convert size to 32bit words */
  *img_size = (*img_size + 3) / 4;

  /* Finish partition header */
  bops->finish_part_hdr(part_hdr, img_size, offs);

  /* Close the file */
  fclose(cfile);

  return SUCCESS;
}

/* Returns an estimation of the output image size */
uint32_t estimate_boot_image_size(bif_cfg_t *bif_cfg) {
  uint8_t i;
  uint32_t estimated_size;
  struct stat st_file;

  /* TODO the offset used hereshould be more
   * dependent on the target architecture */
  estimated_size = BOOTROM_BINS_OFF;

  for (i = 0; i < bif_cfg->nodes_num; i++) {
    /* Skip if param will not include a file */
    if (!bif_cfg->nodes[i].is_file)
      continue;

    if (stat(bif_cfg->nodes[i].fname, &st_file)) {
      errorf("could not stat %s\n", bif_cfg->nodes[i].fname);
      return 0;
    }

    if (bif_cfg->nodes[i].offset)
      estimated_size = bif_cfg->nodes[i].offset;

    estimated_size += st_file.st_size;
  }

  /* Add 3% to make sure padding is covered */
  estimated_size *= 1.03;

  return estimated_size;
}

/* Returns total size of the created image via the last argument.
 * The regular return value is the error code. */
error create_boot_image(uint32_t *img_ptr,
                        bif_cfg_t *bif_cfg,
                        bootrom_ops_t *bops,
                        uint32_t *total_size) {
  /* declare variables */
  bootrom_hdr_t hdr;
  bootrom_offs_t offs;
  uint16_t i, j, f;
  error err;
  int img_term_n = 0;
  uint8_t img_name[BOOTROM_IMG_MAX_NAME_LEN];
  uint8_t pmufw_img[BOOTROM_PMUFW_MAX_SIZE];
  uint32_t pmufw_img_load;
  uint32_t pmufw_img_entry;
  uint32_t pmufw_img_size;
  uint8_t pmufw_img_nbits;
  struct stat pmufile_stat;
  uint8_t part_hdr_count;

  if (bops->append_null_part)
    part_hdr_count = bif_cfg->nodes_num + 1;
  else
    part_hdr_count = bif_cfg->nodes_num;

  bootrom_partition_hdr_t part_hdr[part_hdr_count];
  bootrom_img_hdr_t img_hdr[bif_cfg->nodes_num];
  uint32_t img_size;

  bootrom_img_hdr_tab_t img_hdr_tab;

  img_hdr_tab.hdrs_count = 0;
  for (i = 0; i < bif_cfg->nodes_num; i++) {
    if (bif_cfg->nodes[i].is_file && !bif_cfg->nodes[i].pmufw_image)
      img_hdr_tab.hdrs_count++;
  }

  /* Initialize offsets */
  bops->init_offs(img_ptr, img_hdr_tab.hdrs_count, &offs);

  /* Initialize header */
  bops->init_header(&hdr, &offs);

  /* Iterate through the images and write them */
  for (i = 0, f = 0; i < bif_cfg->nodes_num; i++) {
    /* i - index of all bif nodes
     * f - index of bif node excluding non-file ones */

    /* Skip if param will not include a file */
    if (!bif_cfg->nodes[i].is_file)
      continue;

    if (bif_cfg->nodes[i].pmufw_image) {
      /* For now just assume the firmware is always the maximum length */
      hdr.pmufw_len = BOOTROM_PMUFW_MAX_SIZE;
      hdr.pmufw_total_len = hdr.pmufw_len;

      /* Prepare the array for the firmware */
      memset(pmufw_img, 0x00, sizeof(pmufw_img));

      /* Open pmu file */
      if (stat(bif_cfg->nodes[i].fname, &pmufile_stat)) {
        errorf("could not stat file: %s\n", bif_cfg->nodes[i].fname);
        return ERROR_BOOTROM_NOFILE;
      }
      if (!S_ISREG(pmufile_stat.st_mode)) {
        errorf("not a regular file: %s\n", bif_cfg->nodes[i].fname);
        return ERROR_BOOTROM_NOFILE;
      }
      err = elf_append(pmufw_img,
                       bif_cfg->nodes[i].fname,
                       hdr.pmufw_len,
                       &pmufw_img_size,
                       &pmufw_img_nbits,
                       &pmufw_img_load,
                       &pmufw_img_entry);
      if (err) {
        errorf("failed to parse ELF file: %s\n", bif_cfg->nodes[i].fname);
        return ERROR_BOOTROM_ELF;
      }
      continue;
    }

    if (bif_cfg->nodes[i].offset != 0 &&
        (img_ptr + bif_cfg->nodes[i].offset / sizeof(uint32_t)) < offs.coff) {
      errorf("binary sections overlapping.\n");
      return ERROR_BOOTROM_SEC_OVERLAP;
    } else {
      /* Add 0xFF padding until this binary */
      while (offs.coff < (bif_cfg->nodes[i].offset / sizeof(uint32_t) + img_ptr)) {
        memset(offs.coff, 0xFF, sizeof(uint32_t));
        offs.coff++;
      }
    }

    /* Append file content to memory */
    if (bif_cfg->nodes[i].bootloader && hdr.pmufw_len) {
      /* This is a bootloader image and we have a pmu fw waiting */
      memcpy(offs.coff, pmufw_img, hdr.pmufw_len);
      img_size = hdr.pmufw_len;
    } else {
      img_size = 0;
    }

    err =
      append_file_to_image(offs.coff, bops, &offs, bif_cfg->nodes[i], &(part_hdr[f]), &img_size);

    if (err) {
      return err;
    }

    /* Check if dealing with bootloader (size is in words - thus x 4) */
    if (bif_cfg->nodes[i].bootloader) {
      bops->setup_fsbl_at_curr_off(&hdr, &offs, (part_hdr[f].pd_len * 4) - hdr.pmufw_len);
    }

    /* Update the offset, skip padding for the last image */
    if (i == bif_cfg->nodes_num - 1) {
      offs.coff += part_hdr[f].pd_len;
    } else {
      offs.coff += img_size;
    }

    /* Create image headers for all of them */
    img_hdr[f].part_count = 0x0;

    /* filling this field as a helper */
    img_hdr[f].name_len = strlen(basename(bif_cfg->nodes[i].fname));

    /* Fill the name variable with zeroes */
    memset(img_name, 0x0, BOOTROM_IMG_MAX_NAME_LEN);

    /* Temporarily read the name */
    memcpy(img_name, basename(bif_cfg->nodes[i].fname), img_hdr[f].name_len);

    /* Calculate number of string terminators, this should be 32b
     * however if the name length is divisible by 4 the bootgen
     * binary makes it 64b and thats what we're going to do here */
    if (strlen((char *) img_name) % 4 == 0) {
      img_term_n = 2;
    } else {
      img_term_n = 1;
    }

    /* Make the name len be divisible by 4 */
    while (img_hdr[f].name_len % 4)
      img_hdr[f].name_len++;

    /* The name is packed in big-endian order. To reconstruct
     * the string, unpack 4 bytes at a time, reverse
     * the order, and concatenate. */
    for (j = 0; j < img_hdr[f].name_len; j += 4) {
      img_hdr[f].name[j + 0] = img_name[j + 3];
      img_hdr[f].name[j + 1] = img_name[j + 2];
      img_hdr[f].name[j + 2] = img_name[j + 1];
      img_hdr[f].name[j + 3] = img_name[j + 0];
    }

    /* Append the actual terminators */
    memset(&(img_hdr[f].name[img_hdr[f].name_len]), 0x00, img_term_n * sizeof(uint32_t));

    /* Fill the rest with 0xFF padding */
    for (j = img_hdr[f].name_len + img_term_n * sizeof(uint32_t); j < BOOTROM_IMG_MAX_NAME_LEN;
         j++) {
      img_hdr[f].name[j] = 0xFF;
    }

    /* Name length is not really the length of the name.
     * According to the documentation it is the value of the
     * actual partition count, however the bootgen binary
     * always sets this field to 1. */
    img_hdr[f].name_len = 0x1;

    f++;
  }

  /* Create the image header table */
  bops->init_img_hdr_tab(&img_hdr_tab, img_hdr, part_hdr, &offs);

  /* Copy the image header as all the fields should be filled by now */
  memcpy(offs.hoff, &(img_hdr_tab), sizeof(img_hdr_tab));

  /* Add 0xFF padding until partition header offset */
  while ((uint32_t) (offs.poff - img_ptr) < offs.part_hdr_off / sizeof(uint32_t)) {
    memset(offs.poff, 0xFF, sizeof(uint32_t));
    offs.poff++;
  }

  /* Add null partition at the end */
  if (bops->append_null_part) {
    bootrom_partition_hdr_t *null_hdr = &part_hdr[img_hdr_tab.hdrs_count++];

    memset(null_hdr, 0x0, sizeof(bootrom_partition_hdr_t));
    null_hdr->checksum = 0xffffffff;
  }

  /* Write the partition headers */
  for (i = 0; i < img_hdr_tab.hdrs_count; i++) {
    memcpy(offs.poff, &(part_hdr[i]), sizeof(part_hdr[i]));

    /* Partition header is aligned, so no padding needed */
    offs.poff += sizeof(part_hdr[i]) / sizeof(uint32_t);
  }

  /* Recalculate partition hdr end offset/padding */
  if (offs.part_hdr_end_off) {
    offs.part_hdr_end_off = BOOTROM_PART_HDR_OFF +
                            (img_hdr_tab.hdrs_count * sizeof(struct bootrom_partition_hdr_t)) +
                            BOOTROM_PART_HDR_END_PADD;
  }

  /* Add 0x00 padding until end of partition header */
  while ((uint32_t) (offs.poff - img_ptr) < offs.part_hdr_end_off / sizeof(uint32_t)) {
    memset(offs.poff, 0x00, sizeof(uint32_t));
    offs.poff++;
  }

  /* Add 0xFF padding until BOOTROM_BINS_OFF */
  while ((uint32_t) (offs.poff - img_ptr) < offs.bins_off / sizeof(uint32_t)) {
    memset(offs.poff, 0xFF, sizeof(uint32_t));
    offs.poff++;
  }

  /* Finally write the header to the image */
  memcpy(img_ptr, &(hdr), sizeof(hdr));

  *total_size = offs.coff - img_ptr;

  return SUCCESS;
}
