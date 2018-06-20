#ifndef ELF_H
#define ELF_H

/* Returns the appended file size and the elf header info via arguments.
 * The regular return value is the error code. */
int elf_append(void *addr,
               const char *fname,
               uint32_t img_max_size,
               uint32_t *img_size,
               uint8_t *elf_nbits,
               uint32_t *elf_load,
               uint32_t *elf_entry);

#endif
