#include "crc32.h"

uint32_t crc32(char *message, int length)
{
	int i, j;
	unsigned int byte, crc, mask;

	crc = 0xFFFFFFFF;
	for(i = 0; i < length; i++)
	{
		byte = message[i];
		crc = crc ^ byte;
		for (j = 7; j >= 0; j--)
		{
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
		i = i + 1;
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

void frameAddCRC32(Frame *frame){
	char* char_buf = convert_frame_to_char(frame);
	frame->parity = crc32(char_buf, sizeof(SwpSeqNo) + sizeof(uint16_t) * 2
		+ FRAME_FLAG_SIZE + FRAME_PAYLOAD_SIZE);
	free(char_buf);
}

uint8_t frameIsCorrupted(Frame *frame){
	char* char_buf = convert_frame_to_char(frame);
	if(frame->parity == crc32(char_buf, sizeof(SwpSeqNo) + sizeof(uint16_t) * 2
		+ FRAME_FLAG_SIZE + FRAME_PAYLOAD_SIZE))
		return 0;
	else
		return 1;
}
