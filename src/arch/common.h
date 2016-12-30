#ifndef ARCH_COMMON_H
#define ARCH_COMMON_H

#include <bootrom.h>

int bootrom_init_header(bootrom_hdr_t*);
void bootrom_calc_hdr_checksum(bootrom_hdr_t*);
int bootrom_init_img_hdr_tab(bootrom_img_hdr_tab_t*,
                             bootrom_offs_t*);

#endif
