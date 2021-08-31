#ifndef BITSTREAM_H
#define BITSTREAM_H

/* Check if this really is a bitstream file */
error bitstream_verify(FILE *bitfile);

error bitstream_write_header(FILE *bfile, uint32_t size, const char *design, const char *part);
error bitstream_write(FILE *bfile, uint32_t size, uint32_t *data);

/* Returns the appended bitstream size via the last argument.
 * The regular return value is the error code. */
error bitstream_append(uint32_t *addr, FILE *bitfile, uint32_t *img_size);

#endif
