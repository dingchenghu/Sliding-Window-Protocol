#include "crc32.h"

#define CRCPOLY 0x82f63b78

uint32_t crc32(char *buf, size_t len)
{
	uint32_t crc = 0;
    crc = ~crc;
	int i;
    while (len--) {
        crc ^= *buf++;
        for (i = 0; i < 8; i++)
            crc = crc & 1 ? (crc >> 1) ^ CRCPOLY : crc >> 1;
    }
    return ~crc;
}

/*
typedef uint32_t Parity;

struct Frame_t
{
	SwpSeqNo swpSeqNo; // 1 byte
	uint16_t send_id; // 2 bytes
	uint16_t recv_id; // 2 bytes
	unsigned char flag[FRAME_FLAG_SIZE];
	char data[FRAME_PAYLOAD_SIZE];
	Parity parity; // 4 bytes
};
typedef struct Frame_t Frame;
*/

void frameAddCRC32(Frame *frame)
{
	char* char_buf = convert_frame_to_char(frame);
	frame->parity = crc32(char_buf, sizeof(SwpSeqNo) + sizeof(uint16_t) * 2
		+ FRAME_FLAG_SIZE + FRAME_PAYLOAD_SIZE);
	free(char_buf);
}

uint8_t frameIsCorrupted(Frame *frame)
{
	char* char_buf = convert_frame_to_char(frame);
	if(frame->parity == crc32(char_buf, sizeof(SwpSeqNo) + sizeof(uint16_t) *2 
		+ FRAME_FLAG_SIZE + FRAME_PAYLOAD_SIZE))
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

void ackFrameAddCRC32(Frame *frame)
{
	char* char_buf = convert_frame_to_char(frame);
	frame->parity = crc32(char_buf, sizeof(SwpSeqNo) + sizeof(uint16_t) * 2);
	free(char_buf);
}

uint8_t ackFrameIsCorrupted(Frame *frame)
{
	char* char_buf = convert_frame_to_char(frame);
	if(frame->parity == crc32(char_buf, sizeof(SwpSeqNo) + sizeof(uint16_t) * 2))
	{
		return 0;
	}
	else
	{
		return 1;
	}
}