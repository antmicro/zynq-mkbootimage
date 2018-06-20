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

static int elf_is_loadable_section(const GElf_Shdr *elf_shdr) {
  return elf_shdr->sh_type != SHT_NOBITS &&
         (elf_shdr->sh_flags & SHF_ALLOC) &&
         elf_shdr->sh_size != 0;
}

static int elf_get_startaddr_endaddr(Elf *elf,
                                     uint32_t *start_addr,
                                     uint32_t *end_addr) {
  Elf_Scn *elf_scn = NULL;
  GElf_Shdr elf_shdr;
  *start_addr = -1;
  *end_addr = 0;

  while ((elf_scn = elf_nextscn(elf, elf_scn)) != NULL) {
    if (gelf_getshdr(elf_scn, &elf_shdr) != &elf_shdr)
      return -BOOTROM_ERROR_ELF;

    if (!elf_is_loadable_section(&elf_shdr))
      continue;

    if (*start_addr > elf_shdr.sh_addr)
      *start_addr = elf_shdr.sh_addr;

    if (elf_shdr.sh_addr + elf_shdr.sh_size > *end_addr)
      *end_addr = elf_shdr.sh_addr + elf_shdr.sh_size;
  }

  return BOOTROM_SUCCESS;
}

static int elf_create_image(Elf *elf,
                            uint32_t start_addr,
                            uint8_t *out_buf) {
  Elf_Scn *elf_scn = NULL;
  Elf_Data *elf_data;
  GElf_Shdr elf_shdr;

  while ((elf_scn = elf_nextscn(elf, elf_scn)) != NULL) {
    if (gelf_getshdr(elf_scn, &elf_shdr) != &elf_shdr)
      return -BOOTROM_ERROR_ELF;


    if (!elf_is_loadable_section(&elf_shdr))
      continue;

    elf_data = NULL;

    while ((elf_data = elf_rawdata(elf_scn, elf_data)) != NULL) {
      if (!elf_data->d_buf)
        return -BOOTROM_ERROR_ELF;

      memcpy(out_buf + elf_shdr.sh_addr + elf_data->d_off - start_addr,
             elf_data->d_buf, elf_data->d_size);
    }
  }

  return BOOTROM_SUCCESS;
}

int elf_append(void *addr,
               const char *fname,
               uint32_t img_max_size,
               uint32_t *img_size,
               uint8_t *elf_nbits,
               uint32_t *elf_load,
               uint32_t *elf_entry) {
  int fd_elf;
  Elf *elf;
  GElf_Ehdr elf_ehdr;
  uint32_t start_addr;
  uint32_t end_addr;

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
    elf_end(elf);
    close(fd_elf);
    return -BOOTROM_ERROR_ELF;
  }

  if (elf_get_startaddr_endaddr(elf, &start_addr, &end_addr) != BOOTROM_SUCCESS) {
    elf_end(elf);
    close(fd_elf);
    return -BOOTROM_ERROR_ELF;
  }

  if (end_addr - start_addr > img_max_size) {
    elf_end(elf);
    close(fd_elf);
    return -BOOTROM_ERROR_ELF;
  }

  memset(addr, 0, end_addr - start_addr);

  if (elf_create_image(elf, start_addr, addr) != BOOTROM_SUCCESS) {
    elf_end(elf);
    close(fd_elf);
    return -BOOTROM_ERROR_ELF;
  }

  if (gelf_getehdr(elf, &elf_ehdr) != &elf_ehdr) {
    elf_end(elf);
    close(fd_elf);
    return -BOOTROM_ERROR_ELF;
  }

  *elf_load = start_addr;
  *elf_entry = elf_ehdr.e_entry;
  *elf_nbits = (elf_ehdr.e_ident[EI_CLASS] == ELFCLASS64) ? 64 : 32;
  *img_size = end_addr - start_addr;

  elf_end(elf);
  close(fd_elf);

  return BOOTROM_SUCCESS;
}
