#include "util.h"

//#define NDEBUG
#include <assert.h>

//Linked list functions
int ll_get_length(LLnode * head)
{
    LLnode * tmp;
    int count = 1;
    if (head == NULL)
        return 0;
    else
    {
        tmp = head->next;
        while (tmp != head)
        {
            count++;
            tmp = tmp->next;
        }
        return count;
    }
}

void ll_append_node(LLnode ** head_ptr,
                    void * value)
{
    LLnode * prev_last_node;
    LLnode * new_node;
    LLnode * head;

    if (head_ptr == NULL)
    {
        return;
    }

    //Init the value pntr
    head = (*head_ptr);
    new_node = (LLnode *) malloc(sizeof(LLnode));
    new_node->value = value;

    //The list is empty, no node is currently present
    if (head == NULL)
    {
        (*head_ptr) = new_node;
        new_node->prev = new_node;
        new_node->next = new_node;
    }
    else
    {
        //Node exists by itself
        prev_last_node = head->prev;
        head->prev = new_node;
        prev_last_node->next = new_node;
        new_node->next = head;
        new_node->prev = prev_last_node;
    }
}

void ll_append_node_toFirst(LLnode ** head_ptr,
                    void * value)
{
    ll_append_node(head_ptr, value);
    *head_ptr = ((*head_ptr)->prev);
}

LLnode * ll_pop_node(LLnode ** head_ptr)
{
    LLnode * last_node;
    LLnode * new_head;
    LLnode * prev_head;

    prev_head = (*head_ptr);
    if (prev_head == NULL)
    {
        return NULL;
    }
    last_node = prev_head->prev;
    new_head = prev_head->next;

    //We are about to set the head ptr to nothing because there is only one thing in list
    if (last_node == prev_head)
    {
        (*head_ptr) = NULL;
        prev_head->next = NULL;
        prev_head->prev = NULL;
        return prev_head;
    }
    else
    {
        (*head_ptr) = new_head;
        last_node->next = new_head;
        new_head->prev = last_node;

        prev_head->next = NULL;
        prev_head->prev = NULL;
        return prev_head;
    }
}

//Print out the whole linked list
void print_ll(LLnode * head)
{
    if (head == NULL)
    {
        fprintf(stderr, "Linked list not exist\n");
        return;
    }
    LLnode *tmp = head->next;
    while (tmp != head)
    {
        fprintf(stderr, "%c", (char)tmp->value);
        tmp = tmp->next;
    }
    fprintf(stderr, "\n");
    return;
}

void ll_destroy_node(LLnode * node)
{
    if (node->type == llt_string)
    {
        free((char *) node->value);
    }
    free(node);
}

//Compute the difference in usec for two timeval objects
long timeval_usecdiff(struct timeval *start_time,
                      struct timeval *finish_time)
{
  long usec;
  usec=(finish_time->tv_sec - start_time->tv_sec)*1000000;
  usec+=(finish_time->tv_usec- start_time->tv_usec);
  return usec;
}


//Print out messages entered by the user
void print_cmd(Cmd * cmd)
{
    fprintf(stderr, "src=%d, dst=%d, message=%s\n",
           cmd->src_id,
           cmd->dst_id,
           cmd->message);
}


char * convert_frame_to_char(Frame * frame)
{
    //TODO You should implement this as necessary
    char * char_buffer = (char *) malloc(MAX_FRAME_SIZE);
    memset(char_buffer, 0, MAX_FRAME_SIZE);
    char * curPos = char_buffer;

    memcpy(curPos, &(frame->swpSeqNo), sizeof(SwpSeqNo));
    curPos += sizeof(SwpSeqNo);

    memcpy(curPos, &(frame->send_id), sizeof(uint16_t));
    curPos += sizeof(uint16_t);

    memcpy(curPos, &(frame->recv_id), sizeof(uint16_t));
    curPos += sizeof(uint16_t);

    memcpy(curPos, frame->flag, FRAME_FLAG_SIZE);
    curPos += FRAME_FLAG_SIZE;

    memcpy(curPos, frame->data, FRAME_PAYLOAD_SIZE);
    curPos += FRAME_PAYLOAD_SIZE;

    memcpy(curPos, &(frame->parity), sizeof(Parity));
    curPos += sizeof(Parity);

    assert(curPos - char_buffer == MAX_FRAME_SIZE);

    return char_buffer;
}


Frame * convert_char_to_frame(char * char_buf)
{
    //TODO You should implement this as necessary
    Frame * frame = (Frame *) malloc(sizeof(Frame));
    memset(frame, 0, sizeof(Frame));
    char * curPos = char_buf;

    memcpy(&(frame->swpSeqNo), curPos, sizeof(SwpSeqNo));
    curPos += sizeof(SwpSeqNo);

    memcpy(&(frame->send_id), curPos, sizeof(uint16_t));
    curPos += sizeof(uint16_t);

    memcpy(&(frame->recv_id), curPos, sizeof(uint16_t));
    curPos += sizeof(uint16_t);

    memcpy(frame->flag, curPos, FRAME_FLAG_SIZE);
    curPos += FRAME_FLAG_SIZE;

    memcpy(frame->data, curPos, FRAME_PAYLOAD_SIZE);
    curPos += FRAME_PAYLOAD_SIZE;

    memcpy(&(frame->parity), curPos, sizeof(Parity));
    curPos += sizeof(Parity);

    assert(curPos - char_buf == MAX_FRAME_SIZE);

    return frame;
}

void printFrame(Frame* f)
{
    fprintf(stderr, "SeqNo: %d, SendID: %d, RecvID: %d, Flag: %X%X%X, Parity %X, Data: %s\n",
        f->swpSeqNo, f->send_id, f->recv_id,
        f->flag[0], f->flag[1], f->flag[2],
        f->parity, f->data);
}

uint8_t SwpSeqNo_minus(SwpSeqNo a, SwpSeqNo b)
{
    if(a >= b)
        return a - b;
    else
        return 255 - b + a + 1;
}

//return if ta < tb (earlier)
uint8_t timevalLess(struct timeval ta, struct timeval tb)
{
    if(ta.tv_sec < tb.tv_sec)
        return 1;
    else if(ta.tv_sec > tb.tv_sec)
        return 0;
    else if(ta.tv_usec < tb.tv_usec)
        return 1;
    else
        return 0;
}
