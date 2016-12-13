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

/* Returns the appended bitstream size via the last argument.
 * The regular return value is the error code. */
int append_bitstream(uint32_t *addr, FILE *bitfile, uint32_t *img_size) {
  uint32_t *dest = addr;
  uint8_t section_size;
  char section_data[255];
  char old_val[4];
  char new_val[4];
  char section_hdr[2];
  unsigned int i;

  /* Skip the header - it is already checked */
  fseek(bitfile, FILE_XILINXBIT_SEC_START, SEEK_SET);
  while (1) {
    fread(&section_hdr, 1, sizeof(section_hdr), bitfile);
    if (section_hdr[1] != 0x1 && section_hdr[1] != 0x0) {
      fclose(bitfile);
      fprintf(stderr, "Bitstream file seems to have mismatched sections.\n");
      return BOOTROM_ERROR_BITSTREAM;
    }

    if (section_hdr[0] == FILE_XILINXBIT_SEC_DATA)
      break;

    fread(&section_size, 1, sizeof(uint8_t), bitfile);
    fread(&section_data, 1, section_size, bitfile);
  }

  fread(img_size, 1, 3, bitfile);
  *img_size = ((*img_size >> 16) & 0xFF) |
               (*img_size & 0xFF00) |
               ((*img_size << 16) & 0xFF0000);

  for (i = 0; i <= *img_size; i+=sizeof(old_val)) {
    fread(&old_val, 1, sizeof(old_val), bitfile);
    new_val[0] = old_val[3];
    new_val[1] = old_val[2];
    new_val[2] = old_val[1];
    new_val[3] = old_val[0];
    memcpy(dest, new_val, 4);
    dest++;
  }

  return BOOTROM_SUCCESS;
}

/* Returns the offset by which the addr parameter should be moved
 * and partition header info via argument pointers.
 * The regular return value is the error code. */
int append_file_to_image(uint32_t *addr,
                         bif_node_t node,
                         bootrom_partition_hdr_t *part_hdr,
                         uint32_t *img_size) {
  uint32_t file_header;
  struct stat cfile_stat;
  FILE *cfile;
  Elf *elf;
  int fd_elf;
  size_t elf_hdr_n;
  GElf_Phdr elf_phdr;
  linux_image_header_t linux_img;
  int ret;
  unsigned int i;

  *img_size = 0;

  if(stat(node.fname, &cfile_stat)) {
    fprintf(stderr, "Could not stat file: %s\n", node.fname);
    return -BOOTROM_ERROR_NOFILE;
  }
  if (!S_ISREG(cfile_stat.st_mode)) {
    fprintf(stderr, "Not a regular file: %s\n", node.fname);
    return -BOOTROM_ERROR_NOFILE;
  }
  cfile = fopen(node.fname, "rb");

  if (cfile == NULL) {
    fprintf(stderr, "Could not open file: %s\n", node.fname);
    return -BOOTROM_ERROR_NOFILE;
  }

  /* Clean attribute bits */
  part_hdr->attributes = 0;

  /* Check file format */
  fread(&file_header, 1, sizeof(file_header), cfile);

  printf("processing: %s\n", node.fname);

  switch(file_header) {
  case FILE_MAGIC_ELF:
    /* init elf library */
    if(elf_version(EV_CURRENT) == EV_NONE ) {
      fprintf(stderr, "ELF library initialization failed\n");
      /* Close the file */
      fclose(cfile);
      return -BOOTROM_ERROR_ELF;
    }

    /* open file descriptor used by elf library */
    if (( fd_elf = open(node.fname, O_RDONLY , 0)) < 0) {
      fprintf(stderr, "Could not open file %s.", node.fname);
      /* Close the file */
      fclose(cfile);
      return -BOOTROM_ERROR_ELF;
    }

    /* init elf */
    if (( elf = elf_begin(fd_elf, ELF_C_READ , NULL )) == NULL ) {
      fprintf(stderr, "Not a valid elf file: %s.\n", node.fname);
      /* Close the file */
      fclose(cfile);
      return -BOOTROM_ERROR_ELF;
    }

    /* make sure it is an elf (despite magic byte check) */
    if(elf_kind(elf) != ELF_K_ELF ) {
      fprintf(stderr, "Not a valid elf file: %s.\n", node.fname);
      /* Close the file */
      fclose(cfile);
      return -BOOTROM_ERROR_ELF;
    }

    /* get elf headers count */
    if(elf_getphdrnum(elf, &elf_hdr_n)!= 0) {
      fprintf(stderr, "Not a valid elf file: %s.\n", node.fname);
      /* Close the file */
      fclose(cfile);
      return -BOOTROM_ERROR_ELF;
    }

    /* iterate through all headers to find the executable */
    for(i = 0; i < elf_hdr_n; i ++) {
      if(gelf_getphdr(elf, i, &elf_phdr) != &elf_phdr) {
        fprintf(stderr, "Not a valid elf file: %s.\n", node.fname);
        /* Close the file */
        fclose(cfile);
        return -BOOTROM_ERROR_ELF;
      }

      /* check if the current one has executable flag set */
      if (elf_phdr.p_flags & PF_X) {
        /* this is the one - prepare file for reading */
        fseek(cfile, elf_phdr.p_offset, SEEK_SET);

        /* append the data */
        *img_size = fread(addr, 1, elf_phdr.p_filesz, cfile);

        /* set the load and execution address */
        part_hdr->dest_load_addr = elf_phdr.p_vaddr;
        part_hdr->dest_exec_addr = elf_phdr.p_vaddr;

        /* exit loop */
        break;
      }
    }
    /* close the elf file descriptor */
    elf_end(elf);
    close(fd_elf);

    /* set destination device as the only attribute */
    part_hdr->attributes =
      BOOTROM_PART_ATTR_DEST_DEV_PS << BOOTROM_PART_ATTR_DEST_DEV_OFF;

    break;
  case FILE_MAGIC_XILINXBIT_0:
    /* Xilinx header is 64b, check the other half */
    fread(&file_header, 1, sizeof(file_header), cfile);
    if (file_header != FILE_MAGIC_XILINXBIT_1) {
      fprintf(stderr, "Not a valid bitstream file: %s.\n", node.fname);
      /* Close the file */
      fclose(cfile);
      return BOOTROM_ERROR_BITSTREAM;
    }

    /* It matches, append it to the image */
    ret = append_bitstream(addr, cfile, img_size);
    if (ret != BOOTROM_SUCCESS) {
      return ret;
    }

    /* set destination device as the only attribute */
    part_hdr->attributes =
      BOOTROM_PART_ATTR_DEST_DEV_PL << BOOTROM_PART_ATTR_DEST_DEV_OFF;

    /* no execution address for bitstream */
    part_hdr->dest_load_addr = 0x0;
    part_hdr->dest_exec_addr = 0x0;

    break;
  case FILE_MAGIC_LINUX:
    fseek(cfile, 0, SEEK_SET);
    fread(&linux_img, 1, sizeof(linux_img), cfile);

    fseek(cfile, 0, SEEK_SET);
    *img_size = fread(addr, 1, cfile_stat.st_size, cfile);

    if (linux_img.type == FILE_LINUX_IMG_TYPE_UIM) {
      part_hdr->attributes = BINARY_ATTR_LINUX;
    }

    if (linux_img.type == FILE_LINUX_IMG_TYPE_URD)
      part_hdr->attributes = 0x00; /* despite what the docs say */

    /* set destination device attribute */
    part_hdr->attributes |=
      (BOOTROM_PART_ATTR_DEST_DEV_PS << BOOTROM_PART_ATTR_DEST_DEV_OFF);

    /* No load/execution address */
    part_hdr->dest_load_addr = node.load;
    part_hdr->dest_exec_addr = 0x0;

    break;
  default: /* Treat as a binary file */

    fseek(cfile, 0, SEEK_SET);
    *img_size = fread(addr, 1, cfile_stat.st_size, cfile);

    /* set destination device as the only attribute */
    part_hdr->attributes =
      (0x1 << BOOTROM_PART_ATTR_DEST_DEV_OFF) | BINARY_ATTR_GENERAL;

    /* No load/execution address */
    part_hdr->dest_load_addr = node.load;
    part_hdr->dest_exec_addr = 0x0;
  };

  /* remove trailing zeroes */
  while ( *(addr + (*img_size)) == 0x0 ) {
    (*img_size)--;
  }

  /* uImages actually require some undefined ammount of 0x0 padding after
   * the image so restore some of it in that case */
  if (file_header == FILE_MAGIC_LINUX) {
    if (linux_img.type == FILE_LINUX_IMG_TYPE_UIM) {
      for (i = 0; i < 4; i++) {
        (*img_size)++;
        *(addr + (*img_size)) = 0x0;
      }
    }
  }

  /* The output image needs to use the actual value +1B
   * for some reason */
  part_hdr->pd_word_len = *img_size + 1;
  part_hdr->ed_word_len = *img_size + 1;
  part_hdr->total_word_len = *img_size + 1;

  /* Section count is always set to 1 */
  part_hdr->section_count = 0x1;

  /* Fill remaining fields that don't seem to be used */
  part_hdr->checksum_off = 0x0;
  part_hdr->cert_off = 0x0;
  memset(part_hdr->reserved, 0x00, sizeof(part_hdr->reserved));

  /* Add 0xFF padding */
  while (*img_size % (BOOTROM_IMG_PADDING_SIZE / sizeof(uint32_t))) {
    (*img_size)++;
    memset(addr + (*img_size), 0xFF, sizeof(uint32_t));
  }

  /* Close the file */
  fclose(cfile);

  return BOOTROM_SUCCESS;
}

/* Returns an estimation of the output image size */
uint32_t estimate_boot_image_size(bif_cfg_t *bif_cfg)
{
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
      fprintf(stderr, "Could not stat %s\n", bif_cfg->nodes[i].fname);
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
int create_boot_image(uint32_t *img_ptr,
                      bif_cfg_t *bif_cfg,
                      bootrom_ops_t *bops,
                      uint32_t *total_size) {
  /* declare variables */
  bootrom_hdr_t hdr;
  bootrom_offs_t offs;
  uint16_t i, j, f;
  int ret;
  int img_term_n = 0;
  uint8_t img_name[BOOTROM_IMG_MAX_NAME_LEN];

  bootrom_partition_hdr_t part_hdr[BIF_MAX_NODES_NUM];
  bootrom_img_hdr_t img_hdr[BIF_MAX_NODES_NUM];
  uint32_t img_size;

  bootrom_img_hdr_tab_t img_hdr_tab;

  img_hdr_tab.hdrs_count = 0;

  /* Initialize offsets */
  bops->init_offs(img_ptr, &offs);

  /* Initialize header */
  bops->init_header(&hdr, &offs);

  /* Iterate through the images and write them */
  for (i = 0, f = 0; i < bif_cfg->nodes_num; i++) {
    /* i - index of all bif nodes
     * f - index of bif node excluding non-file ones */

    /* Skip if param will not include a file */
    if (!bif_cfg->nodes[i].is_file)
      continue;

    /* If file - increment headers count */
    img_hdr_tab.hdrs_count++;

    f = img_hdr_tab.hdrs_count - 1;

    if (bif_cfg->nodes[i].offset != 0 &&
        (img_ptr + bif_cfg->nodes[i].offset / sizeof(uint32_t)) < offs.coff) {
      fprintf(stderr, "Binary sections overlapping.\n");
      return -BOOTROM_ERROR_SEC_OVERLAP;
    } else {
      /* Add 0xFF padding until this binary */
      while (offs.coff < (bif_cfg->nodes[i].offset / sizeof(uint32_t) + img_ptr) ) {
        memset(offs.coff, 0xFF, sizeof(uint32_t));
        offs.coff++;
      }
    }

    /* Append file content to memory */
    ret = append_file_to_image(offs.coff,
                               bif_cfg->nodes[i],
                               &(part_hdr[f]),
                               &img_size);

    if (ret != BOOTROM_SUCCESS) {
      return ret;
    }

    /* Check if dealing with bootloader */
    if (bif_cfg->nodes[i].bootloader) {
      /* If so - update the header to point at the correct bootloader */
      hdr.src_offset = (offs.coff - img_ptr) * sizeof(uint32_t);

      /* Image length needs to be in words not bytes */
      hdr.img_len = part_hdr[f].pd_word_len * sizeof(uint32_t);
      hdr.total_img_len = hdr.img_len;

      /* Set target CPU */
      bops->set_target_cpu(&hdr);

      /* Recalculate the checksum */
      bootrom_calc_hdr_checksum(&hdr);
    }

    /* Fill the offset */
    part_hdr[f].data_off = (offs.coff - img_ptr);

    /* Update the offset, skip padding for the last image */
    if (i == bif_cfg->nodes_num - 1) {
      offs.coff += part_hdr[f].pd_word_len;
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
    if (strlen((char*)img_name) % 4 == 0) {
      img_term_n = 2;
    } else {
      img_term_n = 1;
    }

    /* Make the name len be divisible by 4 */
    while(img_hdr[f].name_len % 4)
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
    memset(&(img_hdr[f].name[img_hdr[f].name_len]),
           0x00, img_term_n * sizeof(uint32_t));

    /* Fill the rest with 0xFF padding */
    for (j = img_hdr[f].name_len + img_term_n * sizeof(uint32_t);
         j < BOOTROM_IMG_MAX_NAME_LEN; j++) {
      img_hdr[f].name[j] = 0xFF;
    }

    /* Name length is not really the length of the name.
     * According to the documentation it is the value of the
     * actual partition count, however the bootgen binary
     * always sets this field to 1. */
    img_hdr[f].name_len = 0x1;
  }

  /* Create the image header table */
  bops->init_img_hdr_tab(&img_hdr_tab,
                         img_hdr,
                         part_hdr,
                         &offs);

  /* Copy the image header as all the fields should be filled by now */
  memcpy(offs.hoff, &(img_hdr_tab), sizeof(img_hdr_tab));

  /* Add 0xFF padding until partition header offset */
  while ((uint32_t)(offs.poff - img_ptr) < offs.part_hdr_off / sizeof(uint32_t)) {
    memset(offs.poff, 0xFF, sizeof(uint32_t));
    offs.poff++;
  }

  /* Write the partition headers */
  for (i = 0; i < img_hdr_tab.hdrs_count; i++) {
    memcpy(offs.poff, &(part_hdr[i]), sizeof(part_hdr[i]));

    /* Partition header is aligned, so no padding needed */
    offs.poff += sizeof(part_hdr[i]) / sizeof(uint32_t);
  }

  /* Add 0x00 padding until end of partition header */
  while ((uint32_t)(offs.poff - img_ptr) < offs.part_hdr_end_off / sizeof(uint32_t) ) {
    memset(offs.poff, 0x00, sizeof(uint32_t));
    offs.poff++;
  }

  /* Add 0x00 terminators after partition header
   * if there are more than 3 images */
  if (bif_cfg->nodes_num > 3) {
    for (i = 0; i < 15; ++i) {
      memset(offs.poff, 0x00, sizeof(uint32_t));
      offs.poff++;
    }
  }

  /* Add 0xFF padding until BOOTROM_BINS_OFF */
  while ((uint32_t)(offs.poff - img_ptr) < offs.bins_off / sizeof(uint32_t) ) {
    memset(offs.poff, 0xFF, sizeof(uint32_t));
    offs.poff++;
  }

  /* Finally write the header to the image */
  memcpy(img_ptr, &(hdr), sizeof(hdr));

  *total_size = offs.coff - img_ptr;

  return BOOTROM_SUCCESS;
}
