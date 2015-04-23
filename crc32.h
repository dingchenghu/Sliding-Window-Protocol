#ifndef __CRC32_H__
#define __CRC32_H__

#include <stdint.h>
#include <string.h>
#include "common.h"
#include "util.h"

uint32_t crc32(char *input, int length);

void frameAddCRC32(Frame *frame);
uint8_t frameIsCorrupted(Frame *frame);

#endif
