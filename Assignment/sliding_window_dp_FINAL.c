#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "sliding_window_dp_FINAL.h"

static EVENT_HANDLER(app_ready)
{
    CnetAddr dest;
    size_t len;

    //if, after a series of frames are fired, and the transmitter status of the current sequence number is still set at
    //ACK_WAIT it means that an acknowledgement for the previous set of frame haven't come back
    if(t_status[next_frame] == ACK_WAIT)
        return;

    len = MAX_MESSAGE_SIZE;
    CHECK(CNET_read_application(&dest, (char*) last_msg, &len));

    printf("Destination Node: %d\n", (int)dest);

    send_data_frame(last_msg, len, next_frame, (int)dest);
    t_status[next_frame] = ACK_WAIT; //waiting for acknowledgement for the frame just sent
    next_frame = (next_frame + 1) % MAX_SEQ;
}

//This method is called when a data frame is to be sent from current node to destination node
static void send_data_frame(MSG *msg, size_t len, int seq, int dest)
{
    FRAME f;
    size_t size;
    int link;

    if(len > DATA_SIZE_MAX)
        len = DATA_SIZE_MAX;

    f.seq = seq;
    f.len = len;
    f.type = DATA;
    f.checksum = 0;
    f.dest_node = dest;
    f.src_node = nodeinfo.nodenumber;
    link = routing_table[nodeinfo.nodenumber][f.dest_node]; //get link from routing table
    memcpy(&f.msg, msg, len);
    t_window[seq] = f; //place the frame into the message buffer
    size = FRAME_SIZE(f);
    f.checksum  = CNET_ccitt((unsigned char *) &f, (int)size);

    printf("Transmitting DATA through Link: %d\n", link);
    printf("DATA transmitted,\tFrom Node Number: %d, \tSequence No: %d,\tChecksum: %d\n", f.src_node, seq, f.checksum);

    CHECK(CNET_write_physical(link, (char *) &f, &size));
}

//This method is called when an acknowledgement frame is to be sent, whether it be
//REJ or RR
static void send_ack_frame(int seq, int type, int link)
{
    FRAME f;
    size_t size;

    f.checksum = 0;
    f.src_node = nodeinfo.nodenumber;
    f.type = type;
    if(type == REJ)
        printf("Reject Transmitted, Sequence No: %d, From Node Number: %d, ", f.seq, f.src_node);
    else
        printf("Receive-Ready transmitted, Sequence No: %d, From Node Number: %d, ", f.seq, f.src_node);

    size = sizeof(FRAME);
    f.checksum = CNET_ccitt((unsigned char *) &f, (int)size);

    printf("Checksum: %d\n", f.checksum);

    CHECK(CNET_write_physical(link, (char *) &f, &size));
}

//This method is called when a node has received a frame but it doesn't belong to it
static void middle_man_send(FRAME f, int dest){
    size_t size;
    int link;

    size = FRAME_SIZE(f);
    link = routing_table[nodeinfo.nodenumber][dest]; //get the link from the routing table
    f.checksum = CNET_ccitt((unsigned char *) &f, (int)size);

    printf("\n\nIn Middle Man Node Number: %d\n", nodeinfo.nodenumber);
    printf("\tTransmitting FRAME through Link: %d\n", link);
    printf("\tFRAME transmitted, From Node Number: %d, Sequence No: %d, Checksum: %d\n", nodeinfo.nodenumber, f.seq, f.checksum);
    printf("Exiting Middle Man Node Number: %d\n", nodeinfo.nodenumber);

    CHECK(CNET_write_physical(link, (char *) &f, &size));
}

static EVENT_HANDLER(physical_ready)
{
    FRAME f;
    size_t len;
    int link;
    int checksum;
    int i;

    len = sizeof(FRAME);

    CHECK(CNET_read_physical(&link, (char*) &f, &len));

    checksum = f.checksum;
    f.checksum  = 0;

    //Just compare the incoming checksum with what it actually is to determine if there was some
    //corruption
    printf("\n====================================================================\n");
    printf("Incoming checksum: %d,\tCalculated checksum: %d", checksum, CNET_ccitt((unsigned char*) &f, len));
    printf("\n====================================================================\n");

    if(CNET_ccitt((unsigned char*) &f, len) != checksum)
    {
        // bad checksum, ignore frame
        printf("\t\t\t\tBAD checksum - frame ignored\n");
        send_ack_frame(last_ack_sent + 1, REJ, routing_table[nodeinfo.nodenumber][f.src_node]);
        return;
    }

    if(f.type == DATA){
        if(f.dest_node == nodeinfo.nodenumber){
            r_status[f.seq] = FRM_RECEIVE;
            printf("\t\t\t\tDATA received, seq=%d, From Node Number: %d\n", f.seq, f.src_node);
            //Run through the receiver status to find the last frame that this node received
            //where it breaks the next sequence number to expect
            for(i = (last_ack_sent + 1) % MAX_SEQ; i != f.seq; i = (i + 1) % MAX_SEQ)
            {
                if(r_status[i] != FRM_RECEIVE)
                    break;
            }
            if(i == f.seq)
                send_ack_frame(f.seq, RR, routing_table[nodeinfo.nodenumber][f.src_node]);
        }
        else
            middle_man_send(f, f.dest_node);
    }
    else{
        //If it has received an RR Frame
        if(f.seq == RR)
        {
            printf("\t\t\t\tRR received, seq=%d, From Node Number: %d\n", f.seq, f.src_node);

            int i;
            //Run through the transmitter window to find the last ith acknowledgement which was received
            for(i = (last_ack + 1) % MAX_SEQ; i != f.seq - 1; i = (i + 1) % MAX_SEQ)
            {
                t_status[i] = ACK_RECEIVE;
            }
            last_ack = f.seq - 1;

            //Since it's received this RR, update the transmitter window to show that it's received the frame
            t_status[f.seq-1] = ACK_RECEIVE;
            CNET_stop_timer(wait_timer);
        }
        //If it has received a REJ Frame
        else
        {
            printf("\t\t\t\tREJ received, seq=%d, From Node Number: %d\n", f.seq, f.src_node);
            CNET_stop_timer(wait_timer);
            int i;
            //Send all the frames stored in the message window from sequence number at which the receiver rejected
            for(i = f.seq; i != next_frame; i = (i + 1) % MAX_SEQ)
            {
                //Waiting for acknowledgement for those frames so set to ACK_WAIT
                t_status[i] = ACK_WAIT;
                send_data_frame(&(t_window[i].msg), t_window[i].len, i, f.src_node);
            }
        }
    }
}

//Timer not implemented yet
static EVENT_HANDLER(timer_timeout)
{
    int i;

    printf("Timeout. Resending\n");
    // Acknowledgement not received. Resend from last unacknowledged frame
    for(i = (last_ack + 1) % MAX_SEQ; i != next_frame; i = (i + 1) % MAX_SEQ)
        send_data_frame(&(t_window[i].msg), t_window[i].len, i, t_window[i].dest_node);
    send_data_frame(&(t_window[next_frame].msg), t_window[next_frame].len, i, t_window[next_frame].dest_node);
}

EVENT_HANDLER(reboot_node)
{
    int i;

    last_msg = calloc(1, MAX_MESSAGE_SIZE);
    wait_timer = NULLTIMER;

    for(i = 0; i <= MAX_SEQ; i++){
        t_status[i] = INITIAL_ACK;
        r_status[i] = INITIAL_FRM;
    }

    CHECK(CNET_set_handler(EV_APPLICATIONREADY, app_ready, 0));
    CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
    CHECK(CNET_set_handler(EV_TIMER1, timer_timeout, 0));
    CNET_enable_application(ALLNODES);
}
