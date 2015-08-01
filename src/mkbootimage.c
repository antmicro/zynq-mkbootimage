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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "bif.h"
#include "bootrom.h"

int main(int argc, const char *argv[])
{
  FILE *ofile;
  uint32_t ofile_size;
  uint32_t *file_data;
  bif_cfg_t cfg;
  int ret;
  int i;

  init_bif_cfg(&cfg);

  if (argc != 3) {
    printf("Zynq mkbootimage\n");
    printf("(c) 2013-2015 Antmicro Ltd.\n");
    printf("Usage: mkbootimage <input_bif_file> <output_bit_file>\n");
    return EXIT_FAILURE;
  }

  ret = parse_bif(argv[1], &cfg);
  if (ret != BIF_SUCCESS)
    fprintf(stderr, "Could not parse %s file.\n", argv[1]);

  printf("Nodes found in the %s file:\n", argv[1]);
  for (i = 0; i < cfg.nodes_num; i++) {
    printf(" %s", cfg.nodes[i].fname);
    if (cfg.nodes[i].bootloader)
      printf(" (bootloader)\n");
    else
      printf("\n");
    if (cfg.nodes[i].load)
      printf("  load:   %08x\n", cfg.nodes[i].load);
    if (cfg.nodes[i].offset)
      printf("  offset: %08x\n", cfg.nodes[i].offset);
  }

  /* Generate bin file */
  file_data = malloc (sizeof *file_data * 10000000);

  ofile_size = create_boot_image(file_data, &cfg);

  if (ofile_size < 0) { /* Error */
    free(file_data);
    return ofile_size;
  }

  ofile = fopen(argv[2], "wb");

  if (ofile == NULL ) {
    fprintf(stderr, "Could not open output file: %s\n", argv[2]);
    return EXIT_FAILURE;
  }

  fwrite(file_data, sizeof(uint32_t), ofile_size, ofile);

  fclose(ofile);
  free(file_data);
  deinit_bif_cfg(&cfg);

  printf("All done, quitting\n");
  return EXIT_SUCCESS;
}

