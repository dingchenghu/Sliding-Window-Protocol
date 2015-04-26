#include "sender.h"

//#define NDEBUG
#include <assert.h>

void init_sender(Sender * sender, int id)
{
    pthread_cond_init(&sender->buffer_cv, NULL);
    pthread_mutex_init(&sender->buffer_mutex, NULL);

    //TODO: You should fill in this function as necessary
    sender->send_id = id;
    sender->cur_recv_id = 0;

    sender->input_cmdlist_head = NULL;
    sender->input_framelist_head = NULL;

    sender->LastAckReceived = 0 - 1;
    sender->LastFrameSent = 0 - 1 ;

    sender->SwpWindow = 0;

    memset(sender->framesInWindow, (unsigned char) 0, sizeof(Frame) * SWP_WINDOW_SIZE);

    sender->lastAckNo = 128;
    sender->lastAckNoDuplicateTimes = 0;

    memset(sender->hasSavedSwpState, 0, MAX_COM_ID);
}

void destroy_sender(Sender *sender)
{
    int i;
    for(i = 0; i < MAX_COM_ID; i++)
        if(sender->hasSavedSwpState[i])
            free(sender->SavedSwpStates[i]);
}

/*
struct SenderSwpState_t
{
    SwpSeqNo LastAckReceived;
    SwpSeqNo LastFrameSent;
    uint8_t SwpWindow;
    Frame framesInWindow[SWP_WINDOW_SIZE];
    SwpSeqNo lastAckNo;
    int lastAckNoDuplicateTimes;
};
*/

//switch the swp state for another receiver
void switchReceiver(Sender *sender, uint16_t cur_rec_id, uint16_t new_recv_id)
{
    //save state for current receiver
    sender->SavedSwpStates[cur_rec_id] = (SenderSwpState*) malloc(sizeof(SenderSwpState));

    sender->SavedSwpStates[cur_rec_id]->LastAckReceived = sender->LastAckReceived;
    sender->SavedSwpStates[cur_rec_id]->LastFrameSent = sender->LastFrameSent;
    sender->SavedSwpStates[cur_rec_id]->SwpWindow = sender->SwpWindow;
    memcpy(sender->SavedSwpStates[cur_rec_id]->framesInWindow, sender->framesInWindow,
        sizeof(Frame) * SWP_WINDOW_SIZE);
    sender->SavedSwpStates[cur_rec_id]->lastAckNo = sender->lastAckNo;
    sender->SavedSwpStates[cur_rec_id]->lastAckNoDuplicateTimes = sender->lastAckNoDuplicateTimes;

    sender->hasSavedSwpState[cur_rec_id] = 1;

    //if has saved state
    if(sender->hasSavedSwpState[new_recv_id] == 1)
    {
        sender->LastAckReceived = sender->SavedSwpStates[new_recv_id]->LastAckReceived;
        sender->LastFrameSent = sender->SavedSwpStates[new_recv_id]->LastFrameSent;
        sender->SwpWindow = sender->SavedSwpStates[new_recv_id]->SwpWindow;
        memcpy(sender->framesInWindow, sender->SavedSwpStates[new_recv_id]->framesInWindow,
            sizeof(Frame) * SWP_WINDOW_SIZE);
        sender->lastAckNo = sender->SavedSwpStates[new_recv_id]->lastAckNo;
        sender->lastAckNoDuplicateTimes = sender->SavedSwpStates[new_recv_id]->lastAckNoDuplicateTimes;
    }

    //new init state
    else
    {
        sender->LastAckReceived = 0 - 1;
        sender->LastFrameSent = 0 - 1;
        sender->SwpWindow = 0;
        memset(sender->framesInWindow, (unsigned char) 0, sizeof(Frame) * SWP_WINDOW_SIZE);
        sender->lastAckNo = 128;
        sender->lastAckNoDuplicateTimes = 0;
    }

    sender->cur_recv_id = new_recv_id;
}

struct timeval * sender_get_next_expiring_timeval(Sender * sender)
{
    //TODO: You should fill in this function so that it returns the next timeout that should occur

    struct timeval curr_timeval;
    gettimeofday(&curr_timeval, NULL);

    struct timeval *t = (struct timeval*) malloc(sizeof(struct timeval));

    t->tv_sec = curr_timeval.tv_sec;
    t->tv_usec = curr_timeval.tv_usec + 100000;

    if (t->tv_usec >= 1000000)
    {
        t->tv_sec++;
        t->tv_usec -= 1000000;
    }

    return t;
}

//right shift SWP window by n bits
void rightShiftSWPWindow(Sender * sender, int n){

    assert(n <= SWP_WINDOW_SIZE);

    sender->LastAckReceived += n;
    sender->SwpWindow <<= n;

    //left shift framesInWindow, drop the old backups
    memmove(sender->framesInWindow, sender->framesInWindow + n,
        sizeof(Frame) * (SWP_WINDOW_SIZE - n));
}

// retransimit for current reciever
void retransimit(Sender * sender, LLnode ** outgoing_frames_head_ptr)
{
    if(ll_get_length(*outgoing_frames_head_ptr) >= SWP_WINDOW_SIZE)
        return;

    for(int i = 0; i < SWP_WINDOW_SIZE; i++)
    {
        if(ll_get_length(*outgoing_frames_head_ptr) >= SWP_WINDOW_SIZE)
            break;

        int n = SWP_WINDOW_SIZE - 1 - i;

        if(SwpSeqNo_minus(sender->LastFrameSent, sender->LastAckReceived) <= i)
            break;

        //fprintf(stderr, "Retransimiting...\n");

        //retransimit if ack not received
        if((sender->SwpWindow & (1 << n)) == 0)
        {
            Frame * outgoing_frame = (Frame *) malloc (sizeof(Frame));
            memcpy(outgoing_frame, sender->framesInWindow + i, sizeof(Frame));

            //fprintf(stderr, " SWP = %X, \t%d ", sender->SwpWindow, i);
            //printFrame(outgoing_frame);
            //fprintf(stderr, "\n");
            //frameAddCRC32(outgoing_frame);

            assert(frameIsCorrupted(outgoing_frame) == 0);

            char * outgoing_charbuf = convert_frame_to_char(outgoing_frame);
            ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);

            free(outgoing_frame);
        }
        //fprintf(stderr, "\n");
    }
}

// retransimit for every recievers except current one
void retransimitOthers(Sender * sender, LLnode ** outgoing_frames_head_ptr)
{
    if(ll_get_length(*outgoing_frames_head_ptr) >= SWP_WINDOW_SIZE)
        return;

    for(int r = 0; r < MAX_COM_ID; r++)
    {
        if(ll_get_length(*outgoing_frames_head_ptr) >= SWP_WINDOW_SIZE)
            break;

        if(sender->hasSavedSwpState[r] == 0)
            continue;

        //skip current receiver
        if(r == sender->cur_recv_id)
            continue;

        for(int i = 0; i < SWP_WINDOW_SIZE; i++)
        {
            if(ll_get_length(*outgoing_frames_head_ptr) >= SWP_WINDOW_SIZE)
                break;

            int n = SWP_WINDOW_SIZE - 1 - i;

            if(SwpSeqNo_minus(sender->SavedSwpStates[r]->LastFrameSent,
                sender->SavedSwpStates[r]->LastAckReceived) <= i)
                break;

            if((sender->SavedSwpStates[r]->SwpWindow & (1 << n)) == 0)
            {
                Frame * outgoing_frame = (Frame *) malloc (sizeof(Frame));
                memcpy(outgoing_frame, sender->SavedSwpStates[r]->framesInWindow + i, sizeof(Frame));

                assert(frameIsCorrupted(outgoing_frame) == 0);

                char * outgoing_charbuf = convert_frame_to_char(outgoing_frame);
                ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);

                free(outgoing_frame);

            }
        }
    }
}

void handle_incoming_acks(Sender * sender,
                          LLnode ** outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling incoming ACKs
    //    1) Dequeue the ACK from the sender->input_framelist_head
    //    2) Convert the char * buffer to a Frame data type
    //    3) Check whether the frame is corrupted
    //    4) Check whether the frame is for this sender
    //    5) Do sliding window protocol for sender/receiver pair

    int inframe_queue_length = ll_get_length(sender->input_framelist_head);

    while (inframe_queue_length > 0)
    {
        //Pop a node off the front of the link list and update the count
        LLnode * ll_inmsg_node = ll_pop_node(&sender->input_framelist_head);
        inframe_queue_length = ll_get_length(sender->input_framelist_head);

        char * raw_char_buf = (char *) ll_inmsg_node->value;

        Frame * inframe = convert_char_to_frame(raw_char_buf);
        free(raw_char_buf);

        SwpSeqNo AckNo = inframe->swpSeqNo;

        if(frameIsCorrupted(inframe))
        {
            free(inframe);
            free(ll_inmsg_node);
            continue;
        }

        //assert(inframe->send_id == 0);

        if(inframe->recv_id != sender->send_id)
        {
            free(inframe);
            continue;
        }

        //changed receiver
        if(inframe->send_id != sender->cur_recv_id)
        {
            fprintf(stderr, "Sender #%d, curSender = #%d, newSender = #%d, switching sender...\n\n",
                sender->send_id, sender->cur_recv_id, inframe->send_id);
            switchReceiver(sender, sender->cur_recv_id, inframe->send_id);
        }

        if(AckNo == sender->lastAckNo)
            sender->lastAckNoDuplicateTimes += 1;
        else
        {
            sender->lastAckNo = AckNo;
            sender->lastAckNoDuplicateTimes = 0;
        }

        if(sender->lastAckNoDuplicateTimes > 2)
            retransimit(sender, outgoing_frames_head_ptr);

        if(inframe->recv_id != sender->send_id
            || ((SwpSeqNo_minus(AckNo, sender->LastAckReceived) > SWP_WINDOW_SIZE)
            && SwpSeqNo_minus(AckNo, sender->LastAckReceived) < 128))
        {
            free(inframe);
            free(ll_inmsg_node);
            continue;
        }

        /*
        fprintf(stderr, "Sender %d receiving Ack: %d\n\tCurrent: LFS = %d, LAR = %d, ACK - LAR = %d\n",
            sender->send_id, AckNo,
            sender->LastFrameSent, sender->LastAckReceived,
            SwpSeqNo_minus(AckNo, sender->LastAckReceived));
        */

        //update SWP status
        sender->SwpWindow |= (1 << (SWP_WINDOW_SIZE - SwpSeqNo_minus(AckNo, sender->LastAckReceived)));

        if(SwpSeqNo_minus(AckNo, sender->LastAckReceived) == 1)
        {

            //sender->SwpWindow |= 1 << (SWP_WINDOW_SIZE - 1);

            int n = 1;

            for(int i = SWP_WINDOW_SIZE - 2; i >= 0; i--)
            {
                if((sender->SwpWindow & (1 << i)) == (1 << i))
                    n += 1;
                else
                    break;
            }

            //right shift the SWP window
            //fprintf(stderr, "\tSWP windows flag = %X\n", sender->SwpWindow);
            rightShiftSWPWindow(sender, n);
            //fprintf(stderr, "RightShift SWP window by %d, SWP = %X, new LAR = %d\n\n",
            //    n, sender->SwpWindow, sender->LastAckReceived);
            //fprintf(stderr, "\n");

        }
        // Negative ack
        else if(SwpSeqNo_minus(AckNo, sender->LastAckReceived) == 0
            || SwpSeqNo_minus(AckNo, sender->LastAckReceived) > 128)
        {
            retransimit(sender, outgoing_frames_head_ptr);
        }

        /*
        else if((sender->SwpWindow & (1 << 7)) == (1 << 7))
        {
            int n = 1;
            for(int i = SWP_WINDOW_SIZE - 2; i >= 0; i--){
                if((sender->SwpWindow & (1 << i)) == (1 << i))
                    n += 1;
                else
                    break;
            }

            //right shift the SWP window
            //fprintf(stderr, "\tSWP windows flag = %X\n", sender->SwpWindow);
            rightShiftSWPWindow(sender, n);
            //fprintf(stderr, "RightShift SWP window by %d, new SWP = %X, LAR = %d\n\n",
            //    n, sender->SwpWindow, sender->LastAckReceived);
            //fprintf(stderr, "\n");

        }
        */

        //fprintf(stderr, "\tSWP windows flag = %X\n\n", sender->SwpWindow);

        free(inframe);
        free(ll_inmsg_node);
    }

}

void handle_input_cmds(Sender * sender,
                       LLnode ** outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling input cmd
    //    1) Dequeue the Cmd from sender->input_cmdlist_head
    //    2) Convert to Frame
    //    3) Set up the frame according to the sliding window protocol
    //    4) Compute CRC and add CRC to Frame

    int input_cmd_length = ll_get_length(sender->input_cmdlist_head);

    //Recheck the command queue length to see if stdin_thread dumped a command on us
    input_cmd_length = ll_get_length(sender->input_cmdlist_head);

    /*
    if(input_cmd_length == 0 && sender->LastAckReceived != sender->LastFrameSent)
    {
        retransimit(sender, outgoing_frames_head_ptr);
    }
    */

    while (input_cmd_length > 0)
    {
        if(ll_get_length(*outgoing_frames_head_ptr) >= SWP_WINDOW_SIZE)
            break;

        //pause sending, waiting for acks
        if((SwpSeqNo_minus(sender->LastFrameSent, sender->LastAckReceived) >= SWP_WINDOW_SIZE))
        {
            break;
        }

        //Pop a node off and update the input_cmd_length
        LLnode * ll_input_cmd_node = ll_pop_node(&sender->input_cmdlist_head);
        input_cmd_length = ll_get_length(sender->input_cmdlist_head);

        //Cast to Cmd type and free up the memory for the node
        Cmd * outgoing_cmd = (Cmd *) ll_input_cmd_node->value;
        free(ll_input_cmd_node);

        //not for this sender
        if(outgoing_cmd->src_id != sender->send_id){
            free(outgoing_cmd);
            continue;
        }

        //changed receiver
        if(outgoing_cmd->dst_id != sender->cur_recv_id)
        {
            fprintf(stderr, "Sender #%d, curSender = #%d, newSender = #%d, switching sender...\n\n",
                sender->send_id, sender->cur_recv_id, outgoing_cmd->dst_id);
            switchReceiver(sender, sender->cur_recv_id, outgoing_cmd->dst_id);
        }

        //pause sending, waiting for acks
        if((SwpSeqNo_minus(sender->LastFrameSent, sender->LastAckReceived) >= SWP_WINDOW_SIZE))
        {
            //put the cmd back
            ll_append_node_toFirst(&(sender->input_cmdlist_head), outgoing_cmd);
            break;
        }

        //DUMMY CODE: Add the raw char buf to the outgoing_frames list
        //NOTE: You should not blindly send this message out!
        //      Ask yourself: Is this message actually going to the right receiver (recall that default behavior of send is to broadcast to all receivers)?
        //                    Does the receiver have enough space in in it's input queue to handle this message?
        //                    Were the previous messages sent to this receiver ACTUALLY delivered to the receiver?

        uint8_t hasSubsequent = 0;

        int msg_length = strlen(outgoing_cmd->message);
        if (msg_length > FRAME_PAYLOAD_SIZE)
            hasSubsequent = 1;

        Frame * outgoing_frame = (Frame *) malloc (sizeof(Frame));
        memset(outgoing_frame, 0, sizeof(Frame));

        outgoing_frame->swpSeqNo = ++(sender->LastFrameSent);
        outgoing_frame->send_id = outgoing_cmd->src_id;
        outgoing_frame->recv_id = outgoing_cmd->dst_id;

        if(!hasSubsequent)
        {
            memcpy(outgoing_frame->data, outgoing_cmd->message, strlen(outgoing_cmd->message));

            free(outgoing_cmd->message);
            free(outgoing_cmd);
        }
        else
        {
            memcpy(outgoing_frame->data, outgoing_cmd->message, FRAME_PAYLOAD_SIZE);

            // mark the frame have subsequent
            outgoing_frame->flag[0] ^= (1 << 7);

            //append the remaining msg to the first of ll
            memmove(outgoing_cmd->message, outgoing_cmd->message + FRAME_PAYLOAD_SIZE,
                (msg_length - FRAME_PAYLOAD_SIZE) * sizeof(char));
            memset(outgoing_cmd->message + msg_length - FRAME_PAYLOAD_SIZE,
                '\0', FRAME_PAYLOAD_SIZE * sizeof(char));
            ll_append_node_toFirst(&(sender->input_cmdlist_head), outgoing_cmd);
        }

        frameAddCRC32(outgoing_frame);
        assert(frameIsCorrupted(outgoing_frame) == 0);


        fprintf(stderr, "Sender %d sending a frame: \n\t",
            sender->send_id);
        printFrame(outgoing_frame);

        fprintf(stderr, "\tFrame backuped, LFS = %d, LAR = %d\n\n",
            sender->LastFrameSent, sender->LastAckReceived);

        //assert(sender->LastAckReceived != sender->LastFrameSent);

        //backup frame
        assert(SwpSeqNo_minus(sender->LastFrameSent, sender->LastAckReceived) <= SWP_WINDOW_SIZE);

        if(sender->LastAckReceived != sender->LastFrameSent)
        {
            memcpy(sender->framesInWindow + SwpSeqNo_minus(sender->LastFrameSent, sender->LastAckReceived) - 1,
                outgoing_frame, sizeof(Frame));
        }

        //Convert the message to the outgoing_charbuf
        char * outgoing_charbuf = convert_frame_to_char(outgoing_frame);

        ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);

        free(outgoing_frame);

    }
}


void handle_timedout_frames(Sender * sender,
                            LLnode ** outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling timed out datagrams
    //    1) Iterate through the sliding window protocol information you maintain for each receiver
    //    2) Locate frames that are timed out and add them to the outgoing frames
    //    3) Update the next timeout field on the outgoing frames

    /*
    fprintf(stderr, "Sender %d timeout\n", sender->send_id);
    fprintf(stderr, "\tLFS = %d, LAR = %d\n",
            sender->LastFrameSent, sender-> LastAckReceived);
    */

    if(sender->LastFrameSent == sender-> LastAckReceived){
        //fprintf(stderr, "\tLFS == LAR, do nothing\n");
        retransimitOthers(sender, outgoing_frames_head_ptr);
    }
    else
    {
        //fprintf(stderr, "\tLFS != LAR, retransimitting...\n\tSWP windows flag = %X\n\n",
        //    sender->SwpWindow);
        retransimit(sender, outgoing_frames_head_ptr);
        retransimitOthers(sender, outgoing_frames_head_ptr);
    }
}


void * run_sender(void * input_sender)
{
    struct timespec   time_spec;
    struct timeval    curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Sender * sender = (Sender *) input_sender;
    LLnode * outgoing_frames_head;
    struct timeval * expiring_timeval;
    long sleep_usec_time, sleep_sec_time;

    //This incomplete sender thread, at a high level, loops as follows:
    //1. Determine the next time the thread should wake up
    //2. Grab the mutex protecting the input_cmd/inframe queues
    //3. Dequeues messages from the input queue and adds them to the outgoing_frames list
    //4. Releases the lock
    //5. Sends out the messages

    while(1)
    {
        outgoing_frames_head = NULL;

        //Get the current time
        gettimeofday(&curr_timeval,
                     NULL);

        //time_spec is a data structure used to specify when the thread should wake up
        //The time is specified as an ABSOLUTE (meaning, conceptually, you specify 9/23/2010 @ 1pm, wakeup)
        time_spec.tv_sec  = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;

        //Check for the next event we should handle
        expiring_timeval = sender_get_next_expiring_timeval(sender);

        //Perform full on timeout
        if (expiring_timeval == NULL)
        {
            time_spec.tv_sec += WAIT_SEC_TIME;
            time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        }
        else
        {
            //Take the difference between the next event and the current time
            sleep_usec_time = timeval_usecdiff(&curr_timeval,
                                               expiring_timeval);

            free(expiring_timeval);

            //Sleep if the difference is positive
            assert(sleep_usec_time > 0);

            if (sleep_usec_time > 0)
            {
                sleep_sec_time = sleep_usec_time / 1000000;
                sleep_usec_time = sleep_usec_time % 1000000;
                time_spec.tv_sec += sleep_sec_time;
                time_spec.tv_nsec += sleep_usec_time * 1000;
            }
        }

        //Check to make sure we didn't "overflow" the nanosecond field
        if (time_spec.tv_nsec >= 1000000000)
        {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }


        //*****************************************************************************************
        //NOTE: Anything that involves dequeing from the input frames or input commands should go
        //      between the mutex lock and unlock, because other threads CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&sender->buffer_mutex);

        //Check whether anything has arrived
        int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
        int inframe_queue_length = ll_get_length(sender->input_framelist_head);

        //Nothing (cmd nor incoming frame) has arrived, so do a timed wait on the sender's condition variable (releases lock)
        //A signal on the condition variable will wakeup the thread and reaquire the lock
        if (input_cmd_length == 0 &&
            inframe_queue_length == 0)
        {
            pthread_cond_timedwait(&sender->buffer_cv,
                                   &sender->buffer_mutex,
                                   &time_spec);
        }
        //Implement this
        handle_incoming_acks(sender,
                             &outgoing_frames_head);

        //Implement this
        handle_input_cmds(sender,
                          &outgoing_frames_head);

        pthread_mutex_unlock(&sender->buffer_mutex);


        //Implement this
        handle_timedout_frames(sender,
                               &outgoing_frames_head);

        //CHANGE THIS AT YOUR OWN RISK!
        //Send out all the frames
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);

        assert(ll_outgoing_frame_length <= SWP_WINDOW_SIZE);

        while(ll_outgoing_frame_length > 0)
        {
            LLnode * ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char * char_buf = (char *)  ll_outframe_node->value;

            //Don't worry about freeing the char_buf, the following function does that
            send_msg_to_receivers(char_buf);

            //Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
    return 0;
}
