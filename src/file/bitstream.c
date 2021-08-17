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
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <bootrom.h>
#include <common.h>
#include <file/bitstream.h>

int bitstream_verify(FILE *bitfile) {
  uint32_t fhdr;

  /* Seek to the start of the file */
  rewind(bitfile);

  fread(&fhdr, 1, sizeof(fhdr), bitfile);
  if (fhdr != FILE_MAGIC_XILINXBIT_0)
    return -BOOTROM_ERROR_BITSTREAM;

  /* Xilinx header is 64b, check the other half */
  fread(&fhdr, 1, sizeof(fhdr), bitfile);
  if (fhdr != FILE_MAGIC_XILINXBIT_1)
    return -BOOTROM_ERROR_BITSTREAM;

  /* Both halves match */
  return BOOTROM_SUCCESS;
}

int bitstream_write_header_part(FILE *bitfile,
                              const uint8_t tag,
                              const char *data) {
  uint16_t len = strlen(data) + 1;
  uint8_t n;

  fwrite(&tag, sizeof(uint8_t), 1, bitfile);

  n = (len >> 8) & 0xFF;
  fwrite(&n, sizeof(uint8_t), 1, bitfile);
  n = len & 0xFF;
  fwrite(&n, sizeof(uint8_t), 1, bitfile);

  fwrite(data, sizeof(uint8_t), len, bitfile);

  return 0;
}

int bitstream_write_header(FILE *bitfile,
                           uint32_t size,
                           const char *design,
                           const char *part) {
  const uint8_t header[] = {
    0x00, 0x09, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f,
    0xf0, 0x0f, 0xf0, 0x00, 0x00, 0x01,
  };

  int i;
  uint8_t n;

  char stime[80];
  time_t gtime;
  struct tm ltime;

  gtime = time(NULL);
  ltime = *localtime(&gtime);

  /* TODO: the header sections are similar, generalize it */

  /* Write magic numbers */
  fwrite(header, sizeof(uint8_t), sizeof(header), bitfile);

  /* Write the section 'a' */

  bitstream_write_header_part(bitfile, 'a', design);

  /* Write the section 'b' */
  bitstream_write_header_part(bitfile, 'b', part);

  /* Write the section 'c' */
  strftime(stime, sizeof(stime)-1, "%Y/%m/%d", &ltime);
  bitstream_write_header_part(bitfile, 'c', stime);

  /* Write the section 'd' */
  strftime(stime, sizeof(stime)-1, "%H:%M:%S", &ltime);
  bitstream_write_header_part(bitfile, 'd', stime);

  /* Write the start of the section 'e' */
  n = 'e';
  fwrite(&n, sizeof(uint8_t), 1, bitfile);

  for (i = 3; i >= 0; i--) {
    n = (size >> (i*8)) & 0xFF;
    fwrite(&n, sizeof(uint8_t), 1, bitfile);
  }

  return 0;
}

int bitstream_append(uint32_t *addr, FILE *bitfile, uint32_t *img_size) {
  uint32_t *dest = addr;
  uint32_t chunk, rchunk;
  uint8_t section_size;
  char section_data[255];
  char section_hdr[2];
  unsigned int i;

  /* Skip the header - it is already checked */
  fseek(bitfile, FILE_XILINXBIT_SEC_START, SEEK_SET);
  while (1) {
    fread(&section_hdr, 1, sizeof(section_hdr), bitfile);
    if (section_hdr[1] != 0x1 && section_hdr[1] != 0x0) {
      fclose(bitfile);
      errorf("bitstream file seems to have mismatched sections.\n");
      return -BOOTROM_ERROR_BITSTREAM;
    }

    if (section_hdr[0] == FILE_XILINXBIT_SEC_DATA)
      break;

    fread(&section_size, 1, sizeof(uint8_t), bitfile);
    fread(&section_data, 1, section_size, bitfile);
  }

  fseek(bitfile, -1, SEEK_CUR);
  fread(img_size, 1, 4, bitfile);

  *img_size = __builtin_bswap32(*img_size);
  uint32_t read_size = (*img_size + 3) & ~3;

  for (i = 0; i < read_size; i += sizeof(chunk)) {
    fread(&chunk, 1, sizeof(chunk), bitfile);
    rchunk = __builtin_bswap32(chunk);
    memcpy(dest, &rchunk, sizeof(rchunk));
    dest++;
  }

  return BOOTROM_SUCCESS;
}
