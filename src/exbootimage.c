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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arch/zynq.h>
#include <arch/zynqmp.h>
#include <argp.h>
#include <bif.h>
#include <bootrom.h>
#include <common.h>
#include <file/bitstream.h>
#include <sys/stat.h>

/* A macro constructor of struct fmt */
#define FORMAT(name, type, field, fmt) \
  { name, offsetof(type, field), print_##fmt }

/* Calculate absolute value of a byte or word address */
#define ABS_BADDR(base, addr) ((void *) ((uint8_t *) (base) + (addr)))
#define ABS_WADDR(base, addr) ((void *) ((uint32_t *) (base) + (addr)))

/* Calculate relative byte addres from an absolute address */
#define REL_BADDR(base, addr) ((uint32_t) ((unsigned long) (addr) - (unsigned long) (base)))

/* Check if a byte or word address is correct */
#define IS_REL_BADDR(bsize, addr) ((addr) < (bsize))
#define IS_REL_WADDR(bsize, addr) ((addr) * sizeof(uint32_t) < (bsize))

/* Check if an absolute address is relatively NULL  */
#define IS_REL_NULL(base, addr) ((void *) (addr) <= (void *) (base))

/* A struct for describing binary data layout */
struct format {
  char *name;
  char offset;
  int (*print)(FILE *, void *, int);
};

/* Prepare struct for holding parsed arguments */
struct arguments {
  bool list;
  bool describe;
  bool header;
  bool images;
  bool partitions;
  bool zynqmp;

  bool force;
  bool extract;
  int extract_count;
  char **extract_names;

  char *design;
  char *part;
  bool swap;

  char *fname;
};

/* Type definitions for local usage */
typedef bootrom_img_hdr_t img_hdr_t;
typedef bootrom_partition_hdr_zynq_t zynq_hdr_t;
typedef bootrom_partition_hdr_zynqmp_t zynqmp_hdr_t;
typedef bootrom_hdr_t hdr_t;
typedef bootrom_img_hdr_tab_t img_hdr_tab_t;
typedef bootrom_img_hdr_t img_hdr_t;

typedef bootrom_partition_hdr_t part_hdr_t;

int print_dec(FILE *f, void *base, int offset);
int print_word(FILE *f, void *base, int offset);
int print_dbl_word(FILE *f, void *base, int offset);
int print_name(FILE *f, void *base, int offset);
int print_attr(FILE *f, void *base, int offset);

static int name_to_string(char *dst, void *base, int offset);
static int print_padding(FILE *f, int times, char ch);

static error verify_waddr(void *base, uint32_t size, uint32_t *poffset);
static error get_next_image(void *base, uint32_t size, img_hdr_t **img);

/* Prepare global variables for arg parser */
const char *argp_program_version = MKBOOTIMAGE_VER;
static char doc[] = "Extract data and files from Xilinx Zynq boot images.";
static char args_doc[] =
  "[--zynqmp|-u] "
  "[--extract|-x] "
  "[--list|-l] "
  "[--describe|-d] "
  "[--header|-h] "
  "[--images|-i] "
  "[--parts|-p] "
  "[--bitstream|-bDESIGN,PART-NAME] "
  "[--swap|-s] "
  "<input_bit_file> <files_to_extract>";

static struct argp_option argp_options[] = {
  {"zynqmp", 'u', 0, 0, "Expect files for ZynqMP (default is Zynq)", 0},
  {"extract", 'x', 0, 0, "Extract files embed in the image", 0},
  {"force", 'f', 0, 0, "Don't avoid overwriting an extracted file", 0},
  {"list", 'l', 0, 0, "List files embedded in the image", 0},
  {"describe", 'd', 0, 0, "Describe the boot image (-hip equivalent)", 0},
  {"header", 'h', 0, 0, "Print main boot image header", 0},
  {"images", 'i', 0, 0, "Print partition image headers", 0},
  {"parts", 'p', 0, 0, "Print partition headers", 0},
  {"bitstream", 'b', "DESIGN,PART-NAME", 0, "Reconstruct bitstream with headers on extraction", 0},
  {"swap", 's', 0, 0, "Swap bitstream bytes but don't reconstruct headers", 0},
  {0},
};

/* clang-format off */
static struct format hdr_fmt[] = {
  FORMAT("Width Detection Word",    hdr_t, width_detect,      word),
  FORMAT("Header Signature",        hdr_t, img_id,            word),
  FORMAT("Key Source",              hdr_t, encryption_status, word),
  FORMAT("Header Version",          hdr_t, fsbl_defined_0,    word),
  FORMAT("Source Byte Offset",      hdr_t, src_offset,        word),
  FORMAT("FSBL Image Length",       hdr_t, img_len,           dec),
  FORMAT("FSBL Load Address",       hdr_t, pmufw_total_len,   word),
  FORMAT("FSBL Execution Address",  hdr_t, start_of_exec,     word),
  FORMAT("Total FSBL Length",       hdr_t, total_img_len,     dec),
  FORMAT("QSPI configuration Word", hdr_t, reserved_1,        word),
  FORMAT("Boot Header Checksum",    hdr_t, checksum,          word),
  {0},
};

static struct format img_hdr_tab_fmt[] = {
  FORMAT("Version",                       img_hdr_tab_t, version,          word),
  FORMAT("Header Count",                  img_hdr_tab_t, hdrs_count,       dec),
  FORMAT("Partition Header Offset",       img_hdr_tab_t, part_hdr_off,     word),
  FORMAT("Partition Image Header Offset", img_hdr_tab_t, part_img_hdr_off, word),
  FORMAT("Header Authentication Offset",  img_hdr_tab_t, auth_hdr_off,     word),
  {0},
};

static struct format zynqmp_img_hdr_tab_fmt[] = {
  FORMAT("(ZynqMP) Boot Device", img_hdr_tab_t, boot_dev, word),
  FORMAT("(ZynqMP) Checksum",    img_hdr_tab_t, checksum, word),
  {0},
};

static struct format img_hdr_fmt[] = {
  FORMAT("Next Image Offset",          img_hdr_t, next_img_off, word),
  FORMAT("Partition Header Offset",    img_hdr_t, part_hdr_off, word),
  FORMAT("Partition Count (always 0)", img_hdr_t, part_count,   dec),
  FORMAT("Name Length (usually 1)",    img_hdr_t, name_len,     dec),
  FORMAT("Image Name",                 img_hdr_t, name,         name),
  {0},
};

static struct format zynq_hdr_fmt[] = {
  FORMAT("Encrypted Data Length",   zynq_hdr_t, pd_len,         dec),
  FORMAT("Unencrypted Data Length", zynq_hdr_t, ed_len,         dec),
  FORMAT("Total Length",            zynq_hdr_t, total_len,      dec),
  FORMAT("Load Address",            zynq_hdr_t, dest_load_addr, word),
  FORMAT("Execution Address",       zynq_hdr_t, dest_exec_addr, word),
  FORMAT("Partition Data Offset",   zynq_hdr_t, data_off,       word),
  FORMAT("Attributes",              zynq_hdr_t, attributes,     attr),
  FORMAT("Section Count",           zynq_hdr_t, section_count,  dec),
  FORMAT("Checksum Offset",         zynq_hdr_t, checksum_off,   word),
  FORMAT("Image Header Offset",     zynq_hdr_t, img_hdr_off,    word),
  FORMAT("Certificate Offset",      zynq_hdr_t, cert_off,       word),
  FORMAT("Checksum",                zynq_hdr_t, checksum,       word),
  {0},
};

static struct format zynqmp_hdr_fmt[] = {
  FORMAT("Encrypted Data Length",   zynqmp_hdr_t, pd_len,            dec),
  FORMAT("Unencrypted Data Length", zynqmp_hdr_t, ed_len,            dec),
  FORMAT("Total Length",            zynqmp_hdr_t, total_len,         dec),
  FORMAT("Next Header Offset",      zynqmp_hdr_t, next_part_hdr_off, word),
  FORMAT("Load Address",            zynqmp_hdr_t, dest_load_addr_lo, dbl_word),
  FORMAT("Execution Address",       zynqmp_hdr_t, dest_exec_addr_lo, dbl_word),
  FORMAT("Partition Data Offset",   zynqmp_hdr_t, actual_part_off,   word),
  FORMAT("Attributes",              zynqmp_hdr_t, attributes,        attr),
  FORMAT("Section Count",           zynqmp_hdr_t, section_count,     dec),
  FORMAT("Checksum Offset",         zynqmp_hdr_t, checksum_off,      word),
  FORMAT("Image Header Offset",     zynqmp_hdr_t, img_hdr_off,       word),
  FORMAT("Certificate Offset",      zynqmp_hdr_t, cert_off,          word),
  FORMAT("Checksum",                zynqmp_hdr_t, checksum,          word),
  {0},
};
/* clang-format on */

/* Print a word as a decimal*/
int print_dec(FILE *f, void *base, int offset) {
  return fprintf(f, "%d", *(uint32_t *) ABS_BADDR(base, offset));
}

/* Print a word as a hexadecimal*/
int print_word(FILE *f, void *base, int offset) {
  return fprintf(f, "0x%08x", *(uint32_t *) ABS_BADDR(base, offset));
}

/* Print double word as a decimal*/
int print_dbl_word(FILE *f, void *base, int lo_offset) {
  uint32_t lo, hi;

  lo = *((uint32_t *) ABS_BADDR(base, lo_offset));
  hi = *((uint32_t *) ABS_BADDR(base, lo_offset) + 1);

  return fprintf(f, "0x%08x%08x", hi, lo);
}

/* Print a string of chars encoded as big-endian 32bit words */
int print_name(FILE *f, void *base, int offset) {
  char name[BOOTROM_IMG_MAX_NAME_LEN];

  name_to_string(name, base, offset);
  fputs(name, f);

  return 0;
}

int print_attr(FILE *f, void *base, int offset) {
  mask_name_t *subm;
  uint32_t attr = *(uint32_t *) ABS_BADDR(base, offset);
  uint32_t mask;
  char *name;
  int i;

  if (!attr) {
    print_word(f, base, offset);
    return 0;
  }

  fprintf(f, "explained below\n");

  print_padding(f, 13, ' ');
  fprintf(f, "Hex Value: ");
  print_word(f, base, offset);
  fprintf(f, "\n");

  for (i = 0; bootrom_part_attr_mask_names[i].name; i++) {
    name = bootrom_part_attr_mask_names[i].name;
    mask = bootrom_part_attr_mask_names[i].mask;
    subm = bootrom_part_attr_mask_names[i].submasks;

    print_padding(f, 13, ' ');
    fprintf(f, "%s: %s", name, map_mask_to_name(subm, attr & mask));

    /* Start a newline if there are more attribute values to be printed */
    if (bootrom_part_attr_mask_names[i + 1].name)
      fputc('\n', f);
  }

  return 0;
}

/* Print contents of a struct */
int print_struct(FILE *f, void *base, struct format fmt[]) {
  int i, n, maxlen = 0;

  for (i = 0; fmt[i].name; i++)
    if ((n = strlen(fmt[i].name)) > maxlen)
      maxlen = n;
  for (i = 0; fmt[i].name; i++) {
    fprintf(f, "[0x%08x] %s", fmt[i].offset, fmt[i].name);

    n = strlen(fmt[i].name);
    n = maxlen - n;
    print_padding(f, n, '.');
    fputc(' ', f);

    fmt[i].print(f, base, fmt[i].offset);
    fprintf(f, "\n");
  }

  return 0;
}

/* Print desired number of repetitions of the same char */
int print_padding(FILE *f, int times, char ch) {
  error err = SUCCESS;

  while (times-- > 0)
    if ((err = fputc(ch, f)) < 0)
      return ERROR_CANT_WRITE;
  return SUCCESS;
}

/* Print a section header in program's output */
int print_section(FILE *f, char *section) {
  fprintf(f, "\n%s\n", section);
  print_padding(f, 55, '=');
  fprintf(f, "\n\n");
  return 0;
}

error print_file_list(FILE *f, hdr_t *base, uint32_t size) {
  error err = SUCCESS;
  img_hdr_t *img;

  for (img = NULL; (err = get_next_image(base, size, &img)) == SUCCESS;) {
    print_name(f, img, offsetof(img_hdr_t, name));
    fputc('\n', f);
  }

  return err == ERROR_ITERATION_END ? SUCCESS : err;
}

error print_file_header(FILE *f, hdr_t *base, int zynqmp) {
  img_hdr_tab_t *tab = ABS_BADDR(base, sizeof(hdr_t));

  print_section(f, "MAIN FILE HEADER SECTION");
  print_struct(f, base, hdr_fmt);
  fputc('\n', f);

  print_section(f, "IMAGE HEADER TAB SECTION");
  print_struct(f, tab, img_hdr_tab_fmt);
  if (zynqmp)
    print_struct(f, tab, zynqmp_img_hdr_tab_fmt);
  fputc('\n', f);

  return SUCCESS;
}

error print_image_headers(FILE *f, hdr_t *base, uint32_t size) {
  error err = SUCCESS;
  img_hdr_t *img;

  print_section(f, "IMAGE HEADERS SECTION");
  for (img = NULL; (err = get_next_image(base, size, &img)) == SUCCESS;) {
    print_struct(f, img, img_hdr_fmt);
    fputc('\n', f);
  }

  return err == ERROR_ITERATION_END ? SUCCESS : err;
}

error print_partition_headers(FILE *f, hdr_t *base, uint32_t size, int zynqmp) {
  error err = SUCCESS;
  img_hdr_t *img;
  part_hdr_t *part;

  print_section(f, "PARTITION HEADERS SECTION");
  for (img = NULL; (err = get_next_image(base, size, &img)) == SUCCESS;) {
    if ((err = verify_waddr(base, size, &img->part_hdr_off)))
      return err;
    part = ABS_WADDR(base, img->part_hdr_off);

    print_name(f, img, offsetof(img_hdr_t, name));
    fprintf(f, ":\n");

    if (zynqmp)
      print_struct(f, part, zynqmp_hdr_fmt);
    else
      print_struct(f, part, zynq_hdr_fmt);
    fputc('\n', f);
  }

  return err == ERROR_ITERATION_END ? SUCCESS : err;
}

error print_partition_contents(FILE *f, hdr_t *base, uint32_t size, struct arguments *arguments) {
  error err = SUCCESS;
  uint32_t partsize;
  img_hdr_t *img;
  part_hdr_t *part;
  void *data;
  struct stat bstat;
  FILE *bfile;
  char name[BOOTROM_IMG_MAX_NAME_LEN];

  for (img = NULL; (err = get_next_image(base, size, &img)) == SUCCESS;) {
    name_to_string(name, img, offsetof(img_hdr_t, name));

    /* Check if we're interested */
    if (arguments->extract_names && !is_on_list(arguments->extract_names, name))
      continue;

    /* Get partition header pointer */
    if ((err = verify_waddr(base, size, &img->part_hdr_off)))
      return err;
    part = ABS_WADDR(base, img->part_hdr_off);

    /* Get partition size in bytes */
    partsize = part->total_len;

    /* Get partition data pointer */
    if (arguments->zynqmp) {
      if ((err = verify_waddr(base, size, &((zynqmp_hdr_t *) part)->actual_part_off)))
        return err;
      data = ABS_WADDR(base, ((zynqmp_hdr_t *) part)->actual_part_off);
    } else {
      if ((err = verify_waddr(base, size, &((zynq_hdr_t *) part)->data_off)))
        return err;
      data = ABS_WADDR(base, ((zynq_hdr_t *) part)->data_off);
    }

    /* Check if the file is fine */
    if (!stat(name, &bstat) && !arguments->force) {
      errorf("file %s already exists, use -f to force\n", name);
      return ERROR_BIN_FILE_EXISTS;
    }
    if (!(bfile = fopen(name, "wb"))) {
      errorf("could not open file: %s\n", name);
      return ERROR_BIN_NOFILE;
    }

    fprintf(f, "Extracting %s... ", name);

    /* Treat bitstream files in a separate way */
    if (is_postfix(name, ".bit")) {
      /* Zynq bitstream partisions are appended with an extra noop */
      if (!arguments->zynqmp)
        partsize--;

      /* Reconstruct headers for bit files if it was requested */
      if (arguments->part)
        bitstream_write_header(bfile, partsize, arguments->design, arguments->part);

      /* Perform byteswapped extraction if it was requested */
      if (arguments->swap)
        bitstream_write(bfile, partsize, data);
      else
        fwrite(data, partsize, sizeof(uint32_t), bfile);
    } else {
      fwrite(data, partsize, sizeof(uint32_t), bfile);
    }

    fclose(bfile);

    fprintf(f, "done\n");
  }

  return err == ERROR_ITERATION_END ? SUCCESS : err;
}

/* Convert a name encoded as big-endian 32bit words to string */
static int name_to_string(char *dst, void *base, int offset) {
  int i, j, p = 0;
  char *s = (char *) base + offset;

  for (i = 0; i < BOOTROM_IMG_MAX_NAME_LEN; i += sizeof(uint32_t)) {
    if (*(uint32_t *) (s + i) == 0)
      break;

    for (j = i + 3; j >= i; j--)
      if (s[j] > 0)
        dst[p++] = s[j];
  }
  dst[p] = '\0';

  return p;
}

/* Checkes wether poffset points to a correct word offset */
static error verify_waddr(void *base, uint32_t size, uint32_t *poffset) {
  uint32_t rel;

  if (*poffset < size)
    return SUCCESS;
  rel = REL_BADDR(base, poffset);
  errorf("0x%08x: wrong offset 0x%08x\n", rel, *poffset);
  return ERROR_BIN_WADDR;
}

/* Get next image absolute pointer or pointer to the first one if *img is NULL*/
static error get_next_image(void *base, uint32_t size, img_hdr_t **img) {
  error err;
  img_hdr_tab_t *tab;

  if (!*img) {
    /* Initialize *img if it was NULL */
    tab = ABS_BADDR(base, sizeof(hdr_t));

    if ((err = verify_waddr(base, size, &tab->part_img_hdr_off)))
      return err;
    *img = ABS_WADDR(base, tab->part_img_hdr_off);
  } else if ((err = verify_waddr(base, size, &(*img)->next_img_off))) {
    /* Return error if the next image offset is incorrect */
    return err;
  } else {
    /* Get next image pointer */
    *img = ABS_WADDR((base), (*img)->next_img_off);
  }
  if (IS_REL_NULL(base, *img)) {
    return ERROR_ITERATION_END;
  }
  return SUCCESS;
}

/* Define argument parser */
static error_t argp_parser(int key, char *arg, struct argp_state *state) {
  struct arguments *arguments = state->input;
  char *s;

  switch (key) {
  case 'u':
    arguments->zynqmp = true;
    break;
  case 'x':
    arguments->extract = true;
    break;
  case 'f':
    arguments->force = true;
    break;
  case 'l':
    arguments->list = true;
    break;
  case 'd':
    arguments->header = true;
    arguments->images = true;
    arguments->partitions = true;
    break;
  case 'h':
    arguments->header = true;
    break;
  case 'i':
    arguments->images = true;
    break;
  case 'p':
    arguments->partitions = true;
    break;
  case 's':
    arguments->swap = true;
    break;
  case 'b':
    if (!(s = strchr(arg, ',')))
      argp_usage(state);

    *s = '\0';

    arguments->design = arg;
    arguments->part = s + 1;
    arguments->swap = true;
    break;
  case ARGP_KEY_ARG:
    if (state->arg_num == 0) {
      arguments->fname = arg;
    } else if (arguments->extract) {
      if (arguments->extract_count <= 0) {
        arguments->extract_names = calloc(state->argc + 1, sizeof(char *));
        if (!arguments->extract_names)
          return EXIT_FAILURE;
        arguments->extract_count = 0;
      }
      arguments->extract_names[arguments->extract_count++] = arg;
    } else {
      argp_usage(state);
    }
    break;
  case ARGP_KEY_END:
    if (state->arg_num < 1)
      argp_usage(state);
    else if (arguments->extract_names)
      arguments->extract_names[arguments->extract_count] = NULL;
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

/* Finally initialize argp struct */
static struct argp argp = {argp_options, argp_parser, args_doc, doc, 0, 0, 0};

/* Declare the main function */
int main(int argc, char *argv[]) {
  error err;
  struct arguments arguments;
  struct stat bfile_stat;
  uint32_t size;
  FILE *bfile;
  hdr_t *base;

  /* Init non-string arguments */
  memset(&arguments, 0, sizeof(arguments));

  /* Parse program arguments */
  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  if (stat(arguments.fname, &bfile_stat)) {
    errorf("could not stat file: %s\n", arguments.fname);
    return ERROR_BIN_NOFILE;
  }
  if (!(bfile = fopen(arguments.fname, "rb"))) {
    errorf("could not open file: %s\n", arguments.fname);
    return ERROR_BIN_NOFILE;
  }

  /* Load the image */
  size = bfile_stat.st_size;

  if (!(base = malloc(size)))
    return ERROR_NOMEM;

  fread(base, sizeof(uint8_t), bfile_stat.st_size, bfile);
  fclose(bfile);

  if (arguments.list)
    /* Print partition names */
    if ((err = print_file_list(stdout, base, size)))
      return err;

  if (arguments.header)
    /* Print boot image's header */
    if ((err = print_file_header(stdout, base, arguments.zynqmp)))
      return err;

  if (arguments.images)
    /* Print parition image headers */
    if ((err = print_image_headers(stdout, base, size)))
      return err;

  if (arguments.partitions)
    /* Print partition headers */
    if ((err = print_partition_headers(stdout, base, size, arguments.zynqmp)))
      return err;

  if (arguments.extract)
    /* Write partition contents to files */
    if ((err = print_partition_contents(stdout, base, size, &arguments)))
      return err;

  return EXIT_SUCCESS;
}
