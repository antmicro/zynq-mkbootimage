/* Copyright (c) 2013-2017, Antmicro Ltd
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
#include <fcntl.h>
#include <unistd.h>
#include <gelf.h>

#include <bootrom.h>
#include <file/elf.h>

int elf_find_offsets(const char *fname,
                     uint32_t *elf_start,
                     uint32_t *elf_entry,
                     uint32_t *img_size) {
  Elf *elf;
  int fd_elf;
  Elf_Scn *elf_scn;
  GElf_Shdr elf_shdr;
  GElf_Ehdr elf_ehdr;

  /* Init elf library */
  if (elf_version(EV_CURRENT) == EV_NONE)
    return -BOOTROM_ERROR_ELF;

  /* Open file descriptor used by elf library */
  if ((fd_elf = open(fname, O_RDONLY, 0)) < 0)
    return -BOOTROM_ERROR_ELF;

  /* Init elf */
  if ((elf = elf_begin(fd_elf, ELF_C_READ, NULL)) == NULL) {
    close(fd_elf);
    return -BOOTROM_ERROR_ELF;
  }

  /* Make sure it is an elf (despite magic byte check) */
  if (elf_kind(elf) != ELF_K_ELF) {
    close(fd_elf);
    return -BOOTROM_ERROR_ELF;
  }

  elf_scn = NULL;
  *elf_start = 0;

  while ((elf_scn = elf_nextscn(elf, elf_scn)) != NULL) {
    if (gelf_getshdr(elf_scn, &elf_shdr) != &elf_shdr) {
      elf_end(elf);
      close(fd_elf);
      return -BOOTROM_ERROR_ELF;
    }

    if (*elf_start == 0)
      *elf_start = elf_shdr.sh_offset;

    if (elf_shdr.sh_type == SHT_NOBITS || !(elf_shdr.sh_flags & SHF_ALLOC)) {
      /* Set the final size */
      *img_size = elf_shdr.sh_offset - *elf_start;
      break;
    }
  }

  if (gelf_getehdr(elf, &elf_ehdr) != &elf_ehdr) {
    elf_end(elf);
    close(fd_elf);
    return -BOOTROM_ERROR_ELF;
  }

  *elf_entry = elf_ehdr.e_entry;

  return BOOTROM_SUCCESS;
}

int elf_append(uint32_t *addr,
               FILE *elffile,
               uint32_t *img_size,
               uint32_t start) {

  fseek(elffile, start, SEEK_SET);
  *img_size = fread(addr, 1, *img_size, elffile);

  return BOOTROM_SUCCESS;
}
