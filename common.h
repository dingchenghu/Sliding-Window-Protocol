#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>

#define MAX_COMMAND_LENGTH 16  // 64 Bytes
#define AUTOMATED_FILENAME 512

typedef unsigned char uchar_t;
typedef uint8_t SwpSeqNo;

//System configuration information
struct SysConfig_t
{
    float drop_prob;
    float corrupt_prob;
    unsigned char automated;
    char automated_file[AUTOMATED_FILENAME];
};
typedef struct SysConfig_t  SysConfig;

//Command line input information
struct Cmd_t
{
    uint16_t src_id;
    uint16_t dst_id;
    char * message;
};
typedef struct Cmd_t Cmd;

//Linked list information
enum LLtype
{
    llt_string,
    llt_frame,
    llt_integer,
    llt_head
} LLtype;

struct LLnode_t
{
    struct LLnode_t * prev;
    struct LLnode_t * next;
    enum LLtype type;

    void * value;
};
typedef struct LLnode_t LLnode;

#define SWP_WINDOW_SIZE 8

#define MAX_FRAME_SIZE 64

//TODO: You should change this!
//Remember, your frame can be AT MOST 64 bytes!
#define FRAME_PAYLOAD_SIZE 52
#define FRAME_FLAG_SIZE 3
#define FRAME_PARITY_SIZE 8

typedef uint32_t Parity;

struct Frame_t
{
    SwpSeqNo swpSeqNo; // 1 byte
    uint16_t send_id; // 2 bytes
    uint16_t recv_id; // 2 bytes

    unsigned char flag[FRAME_FLAG_SIZE];
    // flag[0] & (1 << 7) --- has subsequent msg

    char data[FRAME_PAYLOAD_SIZE];
    Parity parity; // 4 bytes
};
typedef struct Frame_t Frame;


//Receiver and sender data structures
struct Receiver_t
{
    //DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_framelist_head
    // 4) recv_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode * input_framelist_head;

    int recv_id;

    SwpSeqNo LastFrameReceived;

    //8 bits -> 8 next SeqNo
    //1 -> seq received
    uint8_t SwpWindow;

    Frame framesInWindow[SWP_WINDOW_SIZE - 1];
};

struct Sender_t
{
    //DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_cmdlist_head
    // 4) input_framelist_head
    // 5) send_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode * input_cmdlist_head;
    LLnode * input_framelist_head;
    int send_id;

    //SeqNum : 0 ~ 255
    SwpSeqNo LastAckReceived;
    SwpSeqNo LastFrameSent;

    //8 bits -> 8 next SeqNo
    //1 -> ack received
    uint8_t SwpWindow;

    Frame framesInWindow[SWP_WINDOW_SIZE];

    SwpSeqNo lastAckNo;
    int lastAckNoduplicateTimes;
};

enum SendFrame_DstType
{
    ReceiverDst,
    SenderDst
} SendFrame_DstType ;

typedef struct Sender_t Sender;
typedef struct Receiver_t Receiver;


//Declare global variables here
//DO NOT CHANGE:
//   1) glb_senders_array
//   2) glb_receivers_array
//   3) glb_senders_array_length
//   4) glb_receivers_array_length
//   5) glb_sysconfig
//   6) CORRUPTION_BITS
Sender * glb_senders_array;
Receiver * glb_receivers_array;
int glb_senders_array_length;
int glb_receivers_array_length;
SysConfig glb_sysconfig;
int CORRUPTION_BITS;

#endif
