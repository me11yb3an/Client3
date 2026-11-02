#ifndef HW_R17G_SAVE_FORMAT_H
#define HW_R17G_SAVE_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#define SAVE_MAGIC 8196
#define SAVE_VERSION_MAX 6

uint32_t save_crc32(const uint8_t *data, size_t length);

/* Longest CRC-valid, structurally accepted prefix, including its CRC word.
 * Version 5 and 6 must parse to the byte before the CRC. Older versions are
 * accepted on magic, version, CRC, and the fixed header; their inventory
 * width lives in game tables this checker does not have.
 * Returns 1 and the prefix length, or 0.
 */
int save_find_image(const uint8_t *data, size_t length, size_t *image_len);

#endif
