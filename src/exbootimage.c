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

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <argp.h>

#include <bif.h>
#include <bootrom.h>

#include <arch/zynq.h>
#include <arch/zynqmp.h>

#include <file/bitstream.h>

#include <sys/stat.h>

/* TODO: handle offset errors */

#define BYTE_OFFSET(base, offset) \
  ((void*)((uint8_t*)(base)+(offset)))

#define WORD_OFFSET(base, offset) \
  ((void*)((uint32_t*)(base)+(offset)))

#define IS_BYTE_OFFSET(bsize, offset) \
  ((offset)/sizeof(uint8_t) < (bsize))

#define IS_WORD_OFFSET(bsize, offset) \
  ((offset)/sizeof(uint32_t) < (bsize))

#define IS_EQUAL(base, relative) \
  ((void*)(base) == (void*)(relative))


/* Prapare struct for holding parsed arguments */
struct arguments {
  uint8_t list;
  uint8_t describe;
  uint8_t header;
  uint8_t images;
  uint8_t partitions;
  uint8_t zynqmp;

  uint8_t force;
  uint8_t extract;
  int extract_count;
  char **extract_names;
  uint8_t in_file_list;

  char *design;
  char *part;

  char *fname;
};

struct format {
  char *name;
  char offset;
  int (*print)(FILE *, void *, int);
};

/* Type definitions for local usage */
typedef bootrom_img_hdr_t              img_hdr_t;
typedef bootrom_partition_hdr_zynq_t   zynq_hdr_t;
typedef bootrom_partition_hdr_zynqmp_t zynqmp_hdr_t;
typedef bootrom_hdr_t                  hdr_t;
typedef bootrom_img_hdr_tab_t          img_hdr_tab_t;
typedef bootrom_img_hdr_t              img_hdr_t;

typedef bootrom_partition_hdr_t        part_hdr_t;

int name_to_string(char *dst, void *base, int offset);
int print_dec(FILE *f, void *base, int offset);
int print_word(FILE *f, void *base, int offset);
int print_dbl_word(FILE *f, void *base, int offset);
int print_name(FILE *f, void *base, int offset);
int print_attr(FILE *f, void *base, int offset);

/* Prepare global variables for arg parser */
const char *argp_program_version = MKBOOTIMAGE_VER;
static char doc[] = "Extract data and files from Xilinx Zynq boot images.";
static char args_doc[] = "[--zynqmp|-u] "
                         "[--extract|-x] "
                         "[--list|-l] "
                         "[--describe|-d] "
                         "[--header|-h] "
                         "[--images|-i] "
                         "[--parts|-p] "
                         "[--bitstream|-b DESIGN,PART]"
                         "<input_bif_file> <output_bin_file>";

static struct argp_option argp_options[] = {
  {"zynqmp",    'u', 0, 0, "Expect files for ZynqMP (default is Zynq)", 0},
  {"extract",   'x', 0, 0, "Extract files embed in the image",          0},
  {"force",     'f', 0, 0, "Don't avoid overwriting an extracted file", 0},
  {"list",      'l', 0, 0, "List files embedded in the image",          0},
  {"describe",  'd', 0, 0, "Describe the boot image (-hip equivalent)", 0},
  {"header",    'h', 0, 0, "Print main boot image header",              0},
  {"images",    'i', 0, 0, "Print partition image headers",             0},
  {"parts",     'p', 0, 0, "Print partition headers",                   0},
  {"bitstream", 'b',
   "DESIGN,PART", 0,
   "Reconstruct bitstream headers after extraction", 0
  },
  { 0 }
};

static struct format hdr_fmt[] = {
  {"Width Detection Word",          offsetof(hdr_t, width_detect),      print_word},
  {"Header Signature",              offsetof(hdr_t, img_id),            print_word},
  {"Key Source",                    offsetof(hdr_t, encryption_status), print_word},
  {"Header Version",                offsetof(hdr_t, fsbl_defined_0),    print_word},
  {"Source Byte Offset",            offsetof(hdr_t, src_offset),        print_word},
  {"FSBL Image Length",             offsetof(hdr_t, img_len),           print_dec},
  {"FSBL Load Address",             offsetof(hdr_t, pmufw_total_len),   print_word},
  {"FSBL Execution Address",        offsetof(hdr_t, start_of_exec),     print_word},
  {"Total FSBL Length",             offsetof(hdr_t, total_img_len),     print_dec},
  {"QSPI configuration Word",       offsetof(hdr_t, reserved_1),        print_word},
  {"Boot Header Checksum",          offsetof(hdr_t, checksum),          print_word},
  { 0 }
};

static struct format img_hdr_tab_fmt[] = {
  {"Version",                       offsetof(img_hdr_tab_t, version),          print_word},
  {"Header Count",                  offsetof(img_hdr_tab_t, hdrs_count),       print_dec},
  {"Partition Header Offset",       offsetof(img_hdr_tab_t, part_hdr_off),     print_word},
  {"Partition Image Header Offset", offsetof(img_hdr_tab_t, part_img_hdr_off), print_word},
  {"Header Authentication Offset",  offsetof(img_hdr_tab_t, auth_hdr_off),     print_word},
  { 0 }
};

static struct format zynqmp_img_hdr_tab_fmt[] = {
  {"(ZynqMP) Boot Device", offsetof(img_hdr_tab_t, boot_dev), print_word},
  {"(ZynqMP) Checksum",    offsetof(img_hdr_tab_t, checksum), print_word},
  { 0 }
};

static struct format img_hdr_fmt[] = {
  {"Next Image Offset",          offsetof(img_hdr_t, next_img_off), print_word},
  {"Partition Header Offset",    offsetof(img_hdr_t, part_hdr_off), print_word},
  {"Partition Count (always 0)", offsetof(img_hdr_t, part_count),   print_dec},
  {"Name Length (usually 1)",    offsetof(img_hdr_t, name_len),     print_dec},
  {"Image Name",                 offsetof(img_hdr_t, name),         print_name},
  { 0 }
};

static struct format zynq_hdr_fmt[] = {
  {"Encrypted Data Length",   offsetof(zynq_hdr_t, pd_len),         print_dec},
  {"Unencrypted Data Length", offsetof(zynq_hdr_t, ed_len),         print_dec},
  {"Total Length",            offsetof(zynq_hdr_t, total_len),      print_dec},
  {"Load Address",            offsetof(zynq_hdr_t, dest_load_addr), print_word},
  {"Execution Address",       offsetof(zynq_hdr_t, dest_exec_addr), print_word},
  {"Partition Data Offset",   offsetof(zynq_hdr_t, data_off),       print_word},
  {"Attributes",              offsetof(zynq_hdr_t, attributes),     print_attr},
  {"Section Count",           offsetof(zynq_hdr_t, section_count),  print_dec},
  {"Checksum Offset",         offsetof(zynq_hdr_t, checksum_off),   print_word},
  {"Image Header Offset",     offsetof(zynq_hdr_t, img_hdr_off),    print_word},
  {"Certificate Offset",      offsetof(zynq_hdr_t, cert_off),       print_word},
  {"Checksum",                offsetof(zynq_hdr_t, checksum),       print_word},
  { 0 }
};

static struct format zynqmp_hdr_fmt[] = {
  {"Encrypted Data Length",   offsetof(zynqmp_hdr_t, pd_len),            print_dec},
  {"Unencrypted Data Length", offsetof(zynqmp_hdr_t, ed_len),            print_dec},
  {"Total Length",            offsetof(zynqmp_hdr_t, total_len),         print_dec},
  {"Next Header Offset",      offsetof(zynqmp_hdr_t, next_part_hdr_off), print_word},
  {"Load Address",            offsetof(zynqmp_hdr_t, dest_load_addr_lo), print_dbl_word},
  {"Execution Address",       offsetof(zynqmp_hdr_t, dest_exec_addr_lo), print_dbl_word},
  {"Partition Data Offset",   offsetof(zynqmp_hdr_t, actual_part_off),   print_word},
  {"Attributes",              offsetof(zynqmp_hdr_t, attributes),        print_attr},
  {"Section Count",           offsetof(zynqmp_hdr_t, section_count),     print_dec},
  {"Checksum Offset",         offsetof(zynqmp_hdr_t, checksum_off),      print_word},
  {"Image Header Offset",     offsetof(zynqmp_hdr_t, img_hdr_off),       print_word},
  {"Certificate Offset",      offsetof(zynqmp_hdr_t, cert_off),          print_word},
  {"Checksum",                offsetof(zynqmp_hdr_t, checksum),          print_word},
  { 0 }
};

/* Print desired number of repetitions of the same char */
int print_padding(FILE *f, int times, char ch) {
  int err;

  while (times-- > 0)
    if ((err = fputc(ch, f)) < 0)
      return err;
  return 0;
}

/* Print a word as a decimal*/
int print_dec(FILE *f, void *base, int offset) {
  return fprintf(f, "%d", *(uint32_t*)BYTE_OFFSET(base, offset));
}

/* Print a word as a hexadecimal*/
int print_word(FILE *f, void *base, int offset) {
  return fprintf(f, "0x%08x", *(uint32_t*)BYTE_OFFSET(base, offset));
}

/* Print double word as a decimal*/
int print_dbl_word(FILE *f, void *base, int lo_offset) {
  uint32_t lo, hi;

  lo = *((uint32_t*)BYTE_OFFSET(base, lo_offset));
  hi = *((uint32_t*)BYTE_OFFSET(base, lo_offset)+1);

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
  uint32_t attr = *(uint32_t*)BYTE_OFFSET(base, offset);

  uint32_t owner =   (attr >> BOOTROM_PART_ATTR_OWNER_OFF)      & 0x3;
  uint32_t rsa =     (attr >> BOOTROM_PART_ATTR_RSA_USED_OFF)   & 0x1;
  uint32_t cpu =     (attr >> BOOTROM_PART_ATTR_DEST_CPU_OFF)   & 0x7;
  uint32_t encrypt = (attr >> BOOTROM_PART_ATTR_ENCRYPTION_OFF) & 0x1;
  uint32_t dev =     (attr >> BOOTROM_PART_ATTR_DEST_DEV_OFF)   & 0x7;
  /*uint32_t exec =    (attr >> BOOTROM_PART_ATTR_A5X_EXEC_S_OFF) & 0x7;*/
  uint32_t exclvl =  (attr >> BOOTROM_PART_ATTR_EXC_LVL_OFF)    & 0x3;
  uint32_t trust =   (attr >> BOOTROM_PART_ATTR_TRUST_ZONE_OFF) & 0x1;

  if (!attr) {
    print_word(f, base, offset);
    return 0;
  }

  fprintf(f, "explained below\n");

  print_padding(f, 13, ' ');
  fprintf(f, "Hex Value: ");
  print_word(f, base, offset);
  fprintf(f, "\n");

  print_padding(f, 13, ' ');
  fprintf(f, "Owner: %s\n",
    owner == 0 ? "FSBL" :
    owner == 1 ? "UBOOT" :
                 "[bad value]");

  print_padding(f, 13, ' ');
  fprintf(f, "RSA: %s\n",
    rsa == 0 ? "not used" :
    rsa == 1 ? "used" :
               "[bad value]");

  print_padding(f, 13, ' ');
  fprintf(f, "Destination CPU: %s\n",
    cpu == 0 ? "none" :
    cpu == 1 ? "A53-0" :
    cpu == 2 ? "A53-1" :
    cpu == 3 ? "A53-2" :
    cpu == 4 ? "A53-3" :
    cpu == 5 ? "R5-0" :
    cpu == 6 ? "R5-1" :
    cpu == 7 ? "R5-L" :
               "[bad value]");

  print_padding(f, 13, ' ');
  fprintf(f, "Encryption: %s\n",
    encrypt == 1 ? "yes" :
    encrypt == 0 ? "no" :
                   "[bad value]");

  print_padding(f, 13, ' ');
  fprintf(f, "Destination Device: %s\n",
    dev == 0 ? "none" :
    dev == 1 ? "PS" :
    dev == 2 ? "PL" :
    dev == 3 ? "INT" :
               "[bad value]");

  /*fprintf(f, "\tA5X Execution S: %s\n"
    exec == 0 ? "64" :
    exec == 1 ? "32" :
                "[bad value]");*/

  print_padding(f, 13, ' ');
  fprintf(f, "Exception Level: %s\n",
    exclvl == 0 ? "0 (el-0)" :
    exclvl == 1 ? "1 (el-1)" :
    exclvl == 2 ? "2 (el-2)" :
    exclvl == 3 ? "3 (el-3)" :
                  "[bad value]");

  print_padding(f, 13, ' ');
  fprintf(f, "Trust Zone: %s",
    trust == 1 ? "yes" :
    trust == 0 ? "no" :
                 "[bad value]");
  return 0;
}

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

/* Convert a name encoded as big-endian 32bit words to string */
int name_to_string(char *dst, void *base, int offset) {
  int i, j, p = 0;
  char *s = (char*)base+offset;

  for (i = 0; i < BOOTROM_IMG_MAX_NAME_LEN; i += sizeof(uint32_t)) {
    if (*(uint32_t*)(s + i) == 0)
      break;

    for (j = i+3; j >= i; j--)
      if (s[j] > 0)
        dst[p++] = s[j];
  }
  dst[p] = '\0';

  return p;
}

/* Check if a string is present on a list*/
int is_on_list(char *list[], char *s) {
  int i;

  for (i = 0; list[i]; i++)
    if (strcmp(list[i], s) == 0)
      return 0xFF;
  return 0;
}

/* Define argument parser */
static error_t argp_parser(int key, char *arg, struct argp_state *state) {
  struct arguments *arguments = state->input;
  char *s;

  switch (key) {
  case 'u':
    arguments->zynqmp = 0xFF;
    break;
  case 'x':
    arguments->extract = 0xFF;
    break;
  case 'f':
    arguments->force = 0xFF;
    break;
  case 'l':
    arguments->list = 0xFF;
    break;
  case 'd':
    arguments->header = 0xFF;
    arguments->images = 0xFF;
    arguments->partitions = 0xFF;
    break;
  case 'h':
    arguments->header = 0xFF;
    break;
  case 'i':
    arguments->images = 0xFF;
    break;
  case 'p':
    arguments->partitions = 0xFF;
    break;
  case 'b':
    if (!(s = strchr(arg, ',')))
      argp_usage(state);

    *s = '\0';

    arguments->design = arg;
    arguments->part = s+1;
    break;
  case ARGP_KEY_ARG:
    if (state->arg_num == 0) {
      arguments->fname = arg;
    } else if (arguments->extract) {
      if (arguments->extract_count <= 0) {
        arguments->extract_names = calloc(state->argc+1, sizeof(char*));
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
    else if (arguments->extract && arguments->extract_count <= 0)
      argp_usage(state);
    else if (arguments->extract)
      arguments->extract_names[arguments->extract_count] = NULL;
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

/* Finally initialize argp struct */
static struct argp argp = { argp_options, argp_parser, args_doc, doc, 0, 0, 0 };

/* Declare the main function */
int main(int argc, char *argv[]) {
  char name[BOOTROM_IMG_MAX_NAME_LEN];
  struct arguments arguments;
  struct stat bfile_stat;
  uint32_t size, offset;
  hdr_t *major;             /* Image's base memory address */
  img_hdr_tab_t *tab;
  img_hdr_t *img, *first_img;
  void *part;
  FILE *bfile;

  /* Init non-string arguments */
  arguments.list = 0;
  arguments.header = 0;
  arguments.images = 0;
  arguments.partitions = 0;
  arguments.zynqmp = 0;
  arguments.fname = NULL;

  arguments.force = 0;
  arguments.extract = 0;
  arguments.extract_count = 0;
  arguments.extract_names = NULL;
  arguments.in_file_list = 0;

  arguments.design = 0;
  arguments.part = 0;

  /* Parse program arguments */
  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  if(stat(arguments.fname, &bfile_stat)) {
    fprintf(stderr, "Could not stat file: %s\n", arguments.fname);
    return -BOOTROM_ERROR_NOFILE;
  }
  if (!S_ISREG(bfile_stat.st_mode)) {
    fprintf(stderr, "Not a regular file: %s\n", arguments.fname);
    return -BOOTROM_ERROR_NOFILE;
  }
  if (!(bfile = fopen(arguments.fname, "rb"))) {
    fprintf(stderr, "Could not open file: %s\n", arguments.fname);
    return -BOOTROM_ERROR_NOFILE;
  }

  /* Load the image */
  size = bfile_stat.st_size;
  major = calloc(size, sizeof(uint8_t));
  fread(major, sizeof(uint8_t), bfile_stat.st_size, bfile);
  fclose(bfile);

  tab = BYTE_OFFSET(major, sizeof(hdr_t));
  first_img = WORD_OFFSET(major, tab->part_img_hdr_off);

  if (arguments.list) {
    for (img = first_img; !IS_EQUAL(major, img);) {
      print_name(stdout, img, offsetof(img_hdr_t, name));
      fputc('\n', stdout);

      img = WORD_OFFSET(major, img->next_img_off);
    }
  }

  if (arguments.header) {
    fprintf(stdout, "HEADER SECTION\n");
    print_padding(stdout, 60, '=');
    fputc('\n', stdout);
    print_struct(stdout, major, hdr_fmt);
    fputc('\n', stdout);

    print_struct(stdout, tab, img_hdr_tab_fmt);
    if (arguments.zynqmp)
      print_struct(stdout, tab, zynqmp_img_hdr_tab_fmt);
    fputc('\n', stdout);
  }

  if (arguments.images) {
    fprintf(stdout, "IMAGE HEADERS SECTION\n");
    print_padding(stdout, 60, '=');
    fputc('\n', stdout);
    for (img = first_img; !IS_EQUAL(major, img);) {
      print_struct(stdout, img, img_hdr_fmt);
      fputc('\n', stdout);

      img = WORD_OFFSET(major, img->next_img_off);
    }
  }

  if (arguments.partitions) {
    fprintf(stdout, "PARTITION HEADER SECTION\n");
    print_padding(stdout, 60, '=');
    fputc('\n', stdout);

    for (img = first_img; !IS_EQUAL(major, img);) {
      part = WORD_OFFSET(major, img->part_hdr_off);

      print_name(stdout, img, offsetof(img_hdr_t, name));
      fprintf(stdout, ":\n");

      if (arguments.zynqmp)
        print_struct(stdout, part, zynqmp_hdr_fmt);
      else
        print_struct(stdout, part, zynq_hdr_fmt);
      fputc('\n', stdout);

      img = WORD_OFFSET(major, img->next_img_off);
    }
  }

  if (arguments.extract) {
    /* Offset of a header field containing partition offset (sic) */
    if (arguments.zynqmp)
      offset = offsetof(zynqmp_hdr_t, actual_part_off);
   else
      offset = offsetof(zynq_hdr_t, data_off);

    for (img = first_img; !IS_EQUAL(major, img);) {
      name_to_string(name, img, offsetof(img_hdr_t, name));

      /* Check if we're interested */
      if (is_on_list(arguments.extract_names, name)) {
        part = WORD_OFFSET(major, img->part_hdr_off);

        size = *(uint32_t*)BYTE_OFFSET(part, offsetof(part_hdr_t, total_len));

        part = WORD_OFFSET(major, *(uint32_t*)BYTE_OFFSET(part, offset));

        if(!stat(name, &bfile_stat) && !arguments.force) {
          fprintf(stderr, "File %s already exists, use -f to force\n", name);
          return -BOOTROM_ERROR_NOFILE;
        }
        if (!(bfile = fopen(name, "wb"))) {
          fprintf(stderr, "Could not open file: %s\n", name);
          return -BOOTROM_ERROR_NOFILE;
        }

        fprintf(stdout, "Extracting %s... ", name);

        if (strcmp(name+strlen(name)-4, ".bit") == 0 && arguments.part)
          bitstream_write_header(
            bfile,
            size,
            arguments.design,
            arguments.part);

        fwrite(part, size, sizeof(uint32_t), bfile);
        fclose(bfile);

        fprintf(stdout, "done\n");
      }

      img = WORD_OFFSET(major, img->next_img_off);
    }
  }

  return EXIT_SUCCESS;
}
