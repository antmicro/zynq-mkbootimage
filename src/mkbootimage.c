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

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <getopt.h>

#include <bif.h>
#include <bootrom.h>

#include <arch/zynq.h>
#include <arch/zynqmp.h>

/* Program info */
const char *program_version = MKBOOTIMAGE_VER;
static char doc[] = "Generate bootloader images for Xilinx Zynq based platforms.";
static char args_doc[] = "[--zynqmp|-u] [--quiet|-q] <input_bif_file> <output_bin_file>";

static struct option longopts[] = {
  { "zynqmp",  no_argument,  NULL,  'u' },
  { "quiet",   no_argument,  NULL,  'q' },
  { NULL,      0,            NULL,  0 }
};

void usage(const char* prog) {
  fprintf(stderr, "%s %s\n", prog, args_doc);
  fprintf(stderr, "%s\n (%s)\n", doc, program_version);
  fprintf(stderr, "  -u : Generate files for ZyqnMP (default is Zynq). Also --zynqmp.\n");
  fprintf(stderr, "  -q : Quiet, no status output (default prints status). Also --quiet.\n");
  exit(1);
}

/* Declare the main function */
int main(int argc, char *argv[]) {
  FILE *ofile;
  const char* prog_name;
  const char *bif_filename, *bin_filename;
  uint32_t ofile_size;
  uint32_t *file_data;
  uint32_t esize, esize_aligned;
  bootrom_ops_t *bops;
  bif_cfg_t cfg;
  bool quiet = false;
  int ret;
  int i;
  int ch;

  /* Defaults for the parser the info about arch */
  cfg.arch = BIF_ARCH_ZYNQ;
  bops = &zynq_bops;

  prog_name = argv[0];

  /* Parse program arguments */
  while ((ch = getopt_long(argc, argv, "uq", longopts, NULL)) != -1) {
    switch (ch) {
      case 'u':
        cfg.arch = BIF_ARCH_ZYNQMP;
        bops = &zynqmp_bops;
        break;
      case 'q':
        quiet = true;
        break;
      default:
        usage(prog_name);
    }
  }

  argc -= optind;
  argv += optind;

  if (argc != 2)
    usage(prog_name);

  bif_filename = argv[0];
  bin_filename = argv[1];

  /* Print program version info */
  if (!quiet)
    printf("%s\n", MKBOOTIMAGE_VER);

  init_bif_cfg(&cfg);

  ret = parse_bif(bif_filename, &cfg);
  if (ret != BIF_SUCCESS || cfg.nodes_num == 0) {
    fprintf(stderr, "Error parsing %s file.\n", bif_filename);
    return EXIT_FAILURE;
  }

  if (!quiet) {
    printf("Nodes found in the %s file:\n", bif_filename);
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
  }

  /* Estimate memory required to fit all the binaries */
  esize = estimate_boot_image_size(&cfg);
  if (!esize)
    return EXIT_FAILURE;

  /* Align estimated size to powers of two */
  esize_aligned = 2;
  while (esize_aligned < esize)
    esize_aligned *= 2;

  /* Allocate memory for output image */
  file_data = malloc(sizeof *file_data * esize_aligned);
  if (!file_data) {
    return -ENOMEM;
  }

  /* Generate bin file */
  ret = create_boot_image(file_data, &cfg, bops, &ofile_size);

  if (ret != BOOTROM_SUCCESS) { /* Error */
    free(file_data);
    return ofile_size;
  }

  ofile = fopen(bin_filename, "wb");

  if (ofile == NULL ) {
    fprintf(stderr, "Could not open output file: %s\n", bin_filename);
    return EXIT_FAILURE;
  }

  fwrite(file_data, sizeof(uint32_t), ofile_size, ofile);

  fclose(ofile);
  free(file_data);
  deinit_bif_cfg(&cfg);

  if (!quiet)
    printf("All done, quitting\n");
  return EXIT_SUCCESS;
}
