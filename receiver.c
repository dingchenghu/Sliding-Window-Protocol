#include "receiver.h"

//#define NDEBUG
#include <assert.h>

void init_receiver(Receiver * receiver, int id)
{
    pthread_cond_init(&receiver->buffer_cv, NULL);
    pthread_mutex_init(&receiver->buffer_mutex, NULL);

    receiver->recv_id = id;
    receiver->cur_send_id = 0;

    receiver->input_framelist_head = NULL;

    receiver->LastFrameReceived = 0 - 1;

    receiver->SwpWindow = 0;

    memset(receiver->framesInWindow, (unsigned char) 0,
        sizeof(Frame) * SWP_WINDOW_SIZE - 1);

    receiver->preMsgHasSubsequent = 0;

    memset(receiver->hasSavedSwpState, 0, MAX_COM_ID);

    receiver->longMsgBufferSize = 0;;
    receiver->longMsgBuffer = NULL;
}

void destroy_receiver(Receiver * receiver)
{
    int i;
    for(i = 0; i < MAX_COM_ID; i++)
        if(receiver->hasSavedSwpState[i])
        {
            //still has unfinished long msg
            if(receiver->SavedSwpStates[i]->preMsgHasSubsequent)
                free(receiver->SavedSwpStates[i]->longMsgBuffer);

            free(receiver->SavedSwpStates[i]);
        }
}

/*
struct ReceiverSwpState_t
{
    SwpSeqNo LastFrameReceived;
    uint8_t SwpWindow;
    Frame framesInWindow[SWP_WINDOW_SIZE - 1];
    uint8_t preMsgHasSubsequent;
};
*/

//switch the swp state for another sender
void switchSender(Receiver *receiver, uint16_t cur_send_id, uint16_t new_send_id)
{
    //save state for current receiver
    if(receiver->hasSavedSwpState[cur_send_id] == 0)
        receiver->SavedSwpStates[cur_send_id] = (ReceiverSwpState*) malloc(sizeof(ReceiverSwpState));

    receiver->SavedSwpStates[cur_send_id]->LastFrameReceived = receiver->LastFrameReceived;
    receiver->SavedSwpStates[cur_send_id]->SwpWindow = receiver->SwpWindow;
    memcpy(receiver->SavedSwpStates[cur_send_id]->framesInWindow, receiver->framesInWindow,
        sizeof(Frame) * (SWP_WINDOW_SIZE - 1));
    receiver->SavedSwpStates[cur_send_id]->preMsgHasSubsequent = receiver->preMsgHasSubsequent;
    receiver->SavedSwpStates[cur_send_id]->longMsgBufferSize = receiver->longMsgBufferSize;
    memcpy(receiver->SavedSwpStates[cur_send_id]->longMsgBuffer,
        receiver->longMsgBuffer, receiver->longMsgBufferSize * sizeof(char));

    receiver->hasSavedSwpState[cur_send_id] = 1;

    //if has saved state
    if(receiver->hasSavedSwpState[new_send_id] == 1)
    {
        receiver->LastFrameReceived = receiver->SavedSwpStates[new_send_id]->LastFrameReceived;
        receiver->SwpWindow = receiver->SavedSwpStates[new_send_id]->SwpWindow;
        memcpy(receiver->framesInWindow, receiver->SavedSwpStates[new_send_id]->framesInWindow,
            sizeof(Frame) * (SWP_WINDOW_SIZE - 1));
        receiver->preMsgHasSubsequent = receiver->SavedSwpStates[new_send_id]->preMsgHasSubsequent;
        receiver->longMsgBufferSize = receiver->SavedSwpStates[new_send_id]->longMsgBufferSize;
        memcpy(receiver->longMsgBuffer, receiver->SavedSwpStates[new_send_id]->longMsgBuffer,
            receiver->SavedSwpStates[new_send_id]->longMsgBufferSize);
    }

    //new init state
    else
    {
        receiver->LastFrameReceived = 0 - 1;
        receiver->SwpWindow = 0;
        memset(receiver->framesInWindow, (unsigned char) 0, sizeof(Frame) * (SWP_WINDOW_SIZE - 1));
        receiver->preMsgHasSubsequent = 0;
        receiver->longMsgBufferSize = 0;;
        receiver->longMsgBuffer = NULL;
    }

    receiver->cur_send_id = new_send_id;
}

void print_msg(Receiver *receiver, Frame *inframe)
{
    //has subsequent
    if(receiver->preMsgHasSubsequent)
    {
        // in long msg
        if((inframe->flag[0] & (1 << 7)) == (1 << 7))
        {
            //printf("%s", inframe->data);
            receiver->longMsgBuffer = (char*) realloc(receiver->longMsgBuffer, receiver->longMsgBufferSize + FRAME_PAYLOAD_SIZE);
            memcpy(receiver->longMsgBuffer + receiver->longMsgBufferSize,
                inframe->data, sizeof(char) * FRAME_PAYLOAD_SIZE);
            receiver->longMsgBufferSize += FRAME_PAYLOAD_SIZE;
            receiver->preMsgHasSubsequent = 1;
        }
        // long msg end
        else
        {
            receiver->longMsgBuffer = (char*) realloc(receiver->longMsgBuffer, receiver->longMsgBufferSize + FRAME_PAYLOAD_SIZE);
            memcpy(receiver->longMsgBuffer + receiver->longMsgBufferSize,
                inframe->data, sizeof(char) * strlen(inframe->data) + 1);
            receiver->longMsgBufferSize += sizeof(char) * strlen(inframe->data);
            

            //print the msg as a single long msg
            /*
            printf("<RECV_%d>:[%s]\n", receiver->recv_id, receiver->longMsgBuffer);
            */
            
            //Partitioning
            int offset = 0;
            while(receiver->longMsgBufferSize >= FRAME_PAYLOAD_SIZE)
            {
                printf("<RECV_%d>:[%.*s]\n", receiver->recv_id, 
                    FRAME_PAYLOAD_SIZE, receiver->longMsgBuffer + offset);
                    offset += FRAME_PAYLOAD_SIZE;
                receiver->longMsgBufferSize -= FRAME_PAYLOAD_SIZE;
            }
            if(receiver->longMsgBufferSize != 0)
                	printf("<RECV_%d>:[%s]\n", receiver->recv_id, 
                        receiver->longMsgBuffer + offset);
            
            receiver->preMsgHasSubsequent = 0;
            receiver->longMsgBufferSize = 0;
            free(receiver->longMsgBuffer);
        }

    }
    // long msg begin
    else if((inframe->flag[0] & (1 << 7)) == (1 << 7))
    {
        //printf("<RECV_%d>:[%s", receiver->recv_id, inframe->data);
        receiver->preMsgHasSubsequent = 1;
        receiver->longMsgBufferSize = FRAME_PAYLOAD_SIZE;
        receiver->longMsgBuffer = (char*) malloc(sizeof(char) * FRAME_PAYLOAD_SIZE);
        memcpy(receiver->longMsgBuffer, inframe->data, sizeof(char) * FRAME_PAYLOAD_SIZE);
    }
    //short msg
    else
        printf("<RECV_%d>:[%s]\n", receiver->recv_id, inframe->data);
}


void handle_incoming_msgs(Receiver * receiver,
                          LLnode ** outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling incoming frames
    //    1) Dequeue the Frame from the sender->input_framelist_head
    //    2) Convert the char * buffer to a Frame data type
    //    3) Check whether the frame is corrupted
    //    4) Check whether the frame is for this receiver
    //    5) Do sliding window protocol for sender/receiver pair

    int incoming_msgs_length = ll_get_length(receiver->input_framelist_head);
    while (incoming_msgs_length > 0)
    {
        //Pop a node off the front of the link list and update the count
        LLnode * ll_inmsg_node = ll_pop_node(&receiver->input_framelist_head);
        incoming_msgs_length = ll_get_length(receiver->input_framelist_head);

        //DUMMY CODE: Print the raw_char_buf
        //NOTE: You should not blindly print messages!
        //      Ask yourself: Is this message really for me?
        //                    Is this message corrupted?
        //                    Is this an old, retransmitted message?
        char * raw_char_buf = (char *) ll_inmsg_node->value;

        Frame * inframe = convert_char_to_frame(raw_char_buf);

        if(frameIsCorrupted(inframe) || inframe->recv_id != receiver->recv_id)
        {
            free(raw_char_buf);
            free(inframe);
            free(ll_inmsg_node);
            continue;
        }

        //assert(inframe->send_id == 0);

        //changed sender
        if(inframe->send_id != receiver->cur_send_id)
        {
            //fprintf(stderr, "Receiver #%d, curSender = #%d, newSender = #%d, switching sender...\n\n",
            //    receiver->recv_id, receiver->cur_send_id, inframe->send_id);
            switchSender(receiver, receiver->cur_send_id, inframe->send_id);
        }

        //not for this receiver / corrupted / SeqNo > Acceptable No
        if((SwpSeqNo_minus(inframe->swpSeqNo, receiver->LastFrameReceived) > SWP_WINDOW_SIZE
            && (SwpSeqNo_minus(inframe->swpSeqNo, receiver->LastFrameReceived) < 128)))
        {
            free(raw_char_buf);
            free(inframe);
            free(ll_inmsg_node);
            continue;
        }

        /*
        fprintf(stderr, "Receiver %d receiving a frame: \n\t", receiver->recv_id);
        printFrame(inframe);
        fprintf(stderr, "\tSending ACK : %d\n", inframe->swpSeqNo);
        fprintf(stderr, "\n");
        */

        //Free raw_char_buf
        free(raw_char_buf);

        uint8_t sendAck = 0;

        if(SwpSeqNo_minus(inframe->swpSeqNo, receiver->LastFrameReceived) == 0)
        {
            sendAck = 1;
            while(0); // do nothing
        }
        //continuous seq no
        else if(SwpSeqNo_minus(inframe->swpSeqNo, receiver->LastFrameReceived) == 1)
        {
            sendAck = 1;
            receiver->LastFrameReceived += 1;

            assert(frameIsCorrupted(inframe) == 0);

            print_msg(receiver, inframe);

            if((receiver->SwpWindow & (1 << 7)) == (1 << 7))
            {
                while((receiver->SwpWindow & (1 << 7)) == (1 << 7))
                {
                    //fprintf(stderr, "in while @ receiver\n");
                    assert(frameIsCorrupted(receiver->framesInWindow) == 0);
                    //printf("<RECV_%d>:[%s]\n", receiver->recv_id, receiver->framesInWindow->data);
                    print_msg(receiver, receiver->framesInWindow);
                    assert(receiver->framesInWindow->swpSeqNo != inframe->swpSeqNo);

                    //left shift framesInWindow
                    memmove(receiver->framesInWindow, receiver->framesInWindow + 1,
                        sizeof(Frame) * (SWP_WINDOW_SIZE - 2));

                    receiver->LastFrameReceived += 1;
                    receiver->SwpWindow <<= 1;
                }
                memmove(receiver->framesInWindow, receiver->framesInWindow + 1,
                    sizeof(Frame) * (SWP_WINDOW_SIZE - 2));
                receiver->SwpWindow <<= 1;
            }
            else
            {
                memmove(receiver->framesInWindow, receiver->framesInWindow + 1,
                    sizeof(Frame) * (SWP_WINDOW_SIZE - 2));
                receiver->SwpWindow <<= 1;
            }

        }
        else if(SwpSeqNo_minus(inframe->swpSeqNo, receiver->LastFrameReceived) <= SWP_WINDOW_SIZE
            && SwpSeqNo_minus(inframe->swpSeqNo, receiver->LastFrameReceived) > 0)
        {
            sendAck = 1;
            SwpSeqNo n = SwpSeqNo_minus(inframe->swpSeqNo, receiver->LastFrameReceived) - 1;
            assert(n >= 1 && n < 8);
            memcpy(receiver->framesInWindow + n - 1, inframe, sizeof(Frame));

            //update SWP window status
            receiver->SwpWindow |= (1 << (SWP_WINDOW_SIZE - n));

            assert((receiver->SwpWindow & 1) == 0);
            //fprintf(stderr, "\tSwpSeqNo = %d, new SwpWindow = %X (LFR = %d)\n\n",
            //    inframe->swpSeqNo, receiver->SwpWindow, receiver->LastFrameReceived);
        }
        else
        {
            sendAck = 1;
            while(0); // do nothing
        }

        //send ack
        Frame *outframe = NULL;
        if(sendAck)
        {
            outframe = (Frame*) malloc(sizeof(Frame));
            outframe->swpSeqNo = inframe->swpSeqNo;
            outframe->send_id = inframe->recv_id;
            outframe->recv_id = inframe->send_id;
            ackFrameAddCRC32(outframe);

            char * outgoing_charbuf = convert_frame_to_char(outframe);
            ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);
        }


        free(inframe);
        free(ll_inmsg_node);

        if(sendAck)
            free(outframe);
    }
}

void * run_receiver(void * input_receiver)
{
    struct timespec   time_spec;
    struct timeval    curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Receiver * receiver = (Receiver *) input_receiver;
    LLnode * outgoing_frames_head;


    //This incomplete receiver thread, at a high level, loops as follows:
    //1. Determine the next time the thread should wake up if there is nothing in the incoming queue(s)
    //2. Grab the mutex protecting the input_msg queue
    //3. Dequeues messages from the input_msg queue and prints them
    //4. Releases the lock
    //5. Sends out any outgoing messages

    while(1)
    {
        //NOTE: Add outgoing messages to the outgoing_frames_head pointer
        outgoing_frames_head = NULL;
        gettimeofday(&curr_timeval,
                     NULL);

        //Either timeout or get woken up because you've received a datagram
        //NOTE: You don't really need to do anything here, but it might be useful for debugging purposes to have the receivers periodically wakeup and print info
        time_spec.tv_sec  = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;
        time_spec.tv_sec += WAIT_SEC_TIME;
        time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        if (time_spec.tv_nsec >= 1000000000)
        {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        //NOTE: Anything that involves dequeing from the input frames should go
        //      between the mutex lock and unlock, because other threads CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&receiver->buffer_mutex);

        //Check whether anything arrived
        int incoming_msgs_length = ll_get_length(receiver->input_framelist_head);
        if (incoming_msgs_length == 0)
        {
            //Nothing has arrived, do a timed wait on the condition variable (which releases the mutex). Again, you don't really need to do the timed wait.
            //A signal on the condition variable will wake up the thread and reacquire the lock
            pthread_cond_timedwait(&receiver->buffer_cv,
                                   &receiver->buffer_mutex,
                                   &time_spec);
        }

        handle_incoming_msgs(receiver,
                             &outgoing_frames_head);

        pthread_mutex_unlock(&receiver->buffer_mutex);

        //CHANGE THIS AT YOUR OWN RISK!
        //Send out all the frames user has appended to the outgoing_frames list
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        while(ll_outgoing_frame_length > 0)
        {
            LLnode * ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char * char_buf = (char *) ll_outframe_node->value;

            //The following function frees the memory for the char_buf object
            send_msg_to_senders(char_buf);

            //Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);

}
