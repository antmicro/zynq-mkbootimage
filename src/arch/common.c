#include <sys/stat.h>
#include <fcntl.h>

#include <bif.h>
#include <bootrom.h>
#include <common.h>
#include <arch/common.h>

/* This calculates the checksum up to (and including) end_addr */

int bootrom_init_header(bootrom_hdr_t *hdr) {
  int i = 0;
  for (i = 0; i < sizeof(hdr->interrupt_table); i++) {
    hdr->interrupt_table[i] = BOOTROM_INT_TABLE_DEFAULT;
  }
  hdr->width_detect = BOOTROM_WIDTH_DETECT;
  memcpy(&(hdr->img_id), BOOTROM_IMG_ID, strlen(BOOTROM_IMG_ID));
  hdr->encryption_status = BOOTROM_ENCRYPTED_NONE;

  /* BootROM does not interpret the field below */
  hdr->src_offset = 0x0; /* Will be filled elsewhere */
  hdr->img_len = 0x0; /* Will be filled elsewhere */
  hdr->reserved_0 = BOOTROM_RESERVED_0;
  hdr->start_of_exec = 0x0; /* Will be filled elsewhere */
  hdr->total_img_len = 0x0; /* Will be filled elsewhere */
  hdr->reserved_1 = BOOTROM_RESERVED_1_RL;

  return BOOTROM_SUCCESS;
}

void bootrom_calc_hdr_checksum(bootrom_hdr_t *hdr) {
  /* the checksum skips the interupt vector and starts
   * at the width detect field */
  hdr->checksum = calc_checksum(&(hdr->width_detect),
                                &(hdr->checksum) - 1);
}

int bootrom_init_img_hdr_tab(bootrom_img_hdr_tab_t *img_hdr_tab,
                             bootrom_img_hdr_t *img_hdr,
                             bootrom_partition_hdr_t *part_hdr,
                             bootrom_offs_t *offs) {
  int i;
  uint32_t img_hdr_size = 0;

  /* Prepare image header table */
  img_hdr_tab->version = BOOTROM_IMG_VERSION;
  img_hdr_tab->part_hdr_off = 0x0; /* filled below */
  img_hdr_tab->part_img_hdr_off = 0x0; /* filled below */
  img_hdr_tab->auth_hdr_off = 0x0; /* auth not implemented */

  /* The data will be copied to the reserved space later
   * when we know all the required offsets,
   * save the pointer for that */
  offs->hoff = offs->poff;
  offs->poff += sizeof(*img_hdr_tab) / sizeof(uint32_t);

  for (i = 0; i < img_hdr_tab->hdrs_count; i++) {
    /* Write 0xFF padding first - will use offset info later */
    img_hdr_size = sizeof(img_hdr[i]) / sizeof(uint32_t);
    while (img_hdr_size % (BOOTROM_IMG_PADDING_SIZE / sizeof(uint32_t))) {
      memset(offs->poff + img_hdr_size, 0xFF, sizeof(uint32_t));
      img_hdr_size++;
    }

    /* calculate the next img hdr offsets */
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
    part_hdr[i].checksum = calc_checksum(&(part_hdr[i].pd_word_len),
                                         &part_hdr[i].reserved[3]);

    if (i == 0) {
      img_hdr_tab->part_img_hdr_off = (offs->poff - offs->img_ptr);
    }

    offs->poff += img_hdr_size;
  }

  /* Fill the partition header offset in img header */
  img_hdr_tab->part_hdr_off = offs->part_hdr_off / sizeof(uint32_t);

  return BOOTROM_SUCCESS;
}
