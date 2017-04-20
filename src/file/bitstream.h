#ifndef BITSTREAM_H
#define BITSTREAM_H

/* Check if this really is a bitstream file */
int bitstream_verify(FILE *bitfile);

/* Returns the appended bitstream size via the last argument.
 * The regular return value is the error code. */
int bitstream_append(uint32_t *addr, FILE *bitfile, uint32_t *img_size);

#endif
