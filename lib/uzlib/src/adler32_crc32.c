/*
 * Standard Adler-32 (RFC 1950) and CRC-32 (zlib, polynomial 0xEDB88320)
 * checksum implementations, matching the uzlib_adler32/uzlib_crc32
 * signatures declared in uzlib.h. Upstream pfalcon/uzlib ships these in
 * separate adler32.c/crc32.c files that aren't vendored into this trimmed
 * copy (only tinflate.c is); this build still needs them because
 * uzlib_uncompress_chksum() calls both unconditionally.
 */

#include "uzlib.h"

#define ADLER32_MOD 65521

uint32_t TINFCC uzlib_adler32(const void *data, unsigned int length, uint32_t prev_sum) {
  const uint8_t *buf = (const uint8_t *)data;
  uint32_t a = prev_sum & 0xffff;
  uint32_t b = (prev_sum >> 16) & 0xffff;

  for (unsigned int i = 0; i < length; i++) {
    a = (a + buf[i]) % ADLER32_MOD;
    b = (b + a) % ADLER32_MOD;
  }

  return (b << 16) | a;
}

uint32_t TINFCC uzlib_crc32(const void *data, unsigned int length, uint32_t crc) {
  const uint8_t *buf = (const uint8_t *)data;

  for (unsigned int i = 0; i < length; i++) {
    crc ^= buf[i];
    for (int bit = 0; bit < 8; bit++) {
      crc = (crc >> 1) ^ (0xEDB88320u & (~(crc & 1) + 1));
    }
  }

  return crc;
}
