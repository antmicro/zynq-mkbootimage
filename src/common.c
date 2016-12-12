#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>

uint32_t calc_checksum(uint32_t *start_addr, uint32_t *end_addr) {
  uint32_t *ptr;
  uint32_t sum;

  sum = 0;
  ptr = start_addr;

  while( ptr <= end_addr) {
    sum += *ptr;
    ptr++;
  }

  return ~sum;
}
