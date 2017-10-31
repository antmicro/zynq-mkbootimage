#include <sys/stat.h>
#include <fcntl.h>

#include <bif.h>
#include <bootrom.h>
#include <common.h>
#include <arch/common.h>

int bootrom_init_header(bootrom_hdr_t *hdr) {
  unsigned int i = 0;
  for (i = 0; i < sizeof(hdr->interrupt_table) / sizeof(hdr->interrupt_table[0]); i++) {
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
                             bootrom_offs_t *offs) {
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

  return BOOTROM_SUCCESS;
}
