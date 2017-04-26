#ifndef ELF_H
#define ELF_H

/* Find all ofsets and sizes that will be needed to properly handle the elf
 * file.
 * Returns the error code */
int elf_find_offsets(const char *fname,
                     uint8_t *elf_nbits,
                     uint32_t *elf_start,
                     uint32_t *elf_entry,
                     uint32_t *img_size);

/* Returns the appended file size and the elf entry point via arguments.
 * The regular return value is the error code. */
int elf_append(uint32_t *addr,
               FILE *elffile,
               uint32_t *img_size,
               uint32_t start);

#endif
