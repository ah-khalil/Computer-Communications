#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "sliding_window_dp_2.h"

static EVENT_HANDLER(app_ready)
{
    CnetAddr dest;
    size_t len;

    if(t_status[next_frame] == ACK_WAIT)
    {
        wait_timer = CNET_start_timer(EV_TIMER1, wait_timer, 0);
        CNET_disable_application(ALLNODES);
        return;
    }

    len = MAX_MESSAGE_SIZE;
    CHECK(CNET_read_application(&dest, (char*) last_msg, &len));

    printf("Destination Node: %d\n", (int)dest);

    send_data_frame(last_msg, len, next_frame, (int)dest);
    t_status[next_frame] = ACK_WAIT;
    next_frame = (next_frame + 1) % (MAXSEQ + 1);
}

static void send_data_frame(MSG *msg, size_t len, int seq, int dest)
{
    DATA_FRAME f;
    size_t size;
    int link;

    if(len > DATA_SIZE_MAX)
        len = DATA_SIZE_MAX;

    f.seq = seq;
    f.len = len;
    f.type = DATA;
    f.checksum = 0;
    f.destnode = dest;
    f.srcnode = nodeinfo.nodenumber;
    link = routing_table[nodeinfo.nodenumber][f.destnode];
    memcpy(&f.msg, msg, len);
    t_window[seq] = f;
    size = DATA_FRAME_SIZE(f);
    f.checksum  = CNET_ccitt((unsigned char *) &f, (int)size);

    printf("Transmitting DATA through Link: %d\n", link);
    printf("DATA transmitted,\tFrom Node Number: %d, \tSequence No: %d,\tChecksum: %d\n", f.srcnode, seq, f.checksum);

    /*printf("\n\n*************************************************");
    printf("\nLength of Original DATA_FRAME: %d", (int)sizeof(f));
    printf("\n*************************************************\n\n");*/


    CHECK(CNET_write_physical_reliable(link, (char *) &f, &size));
}

static void send_ack_frame(int seq, int type, int link)
{
    DATA_FRAME f;
    size_t size;

    f.checksum = 0;
    f.srcnode = nodeinfo.nodenumber;
    f.type = type;
    if(type == REJ){
        f.seq = -seq;
        printf("Reject transmitted,\tSequence No: %d, \tFrom Node Number: %d,\t", f.seq * -1, f.srcnode);
    }
    else{
        f.seq = seq;
        printf("Receive-Ready transmitted,\tSequence No: %d,\tFrom Node Number: %d, \t", f.seq, f.srcnode);
    }

    size = sizeof(DATA_FRAME);
    f.checksum = CNET_ccitt((unsigned char *) &f, (int)size);

    printf("Checksum: %d\n", f.checksum);

    /*printf("\n\n*************************************************");
    printf("\nLength of Acknowledgement DATA_FRAME: %d", (int)sizeof(f));
    printf("\n*************************************************\n\n");*/

    CHECK(CNET_write_physical_reliable(link, (char *) &f, &size));
}

static void middle_man_send(DATA_FRAME f, int dest){
    size_t size;
    int link;

    size = DATA_FRAME_SIZE(f);
    link = routing_table[nodeinfo.nodenumber][dest];
    f.checksum = CNET_ccitt((unsigned char *) &f, (int)size);
    printf("\n\nIn Middle Man Node Number: %d\n", nodeinfo.nodenumber);

    printf("\tTransmitting FRAME through Link: %d\n", link);
    printf("\tFRAME transmitted,\tFrom Node Number: %d, \tSequence No: %d,\tChecksum: %d\n", nodeinfo.nodenumber, f.seq, f.checksum);
    printf("Exiting Middle Man Node Number: %d\n", nodeinfo.nodenumber);

    /*printf("\n\n*************************************************");
    printf("\nLength of Rerouting DATA_FRAME: %d", (int)sizeof(f));
    printf("\n*************************************************\n\n");*/

    CHECK(CNET_write_physical_reliable(link, (char *) &f, &size));
}

static EVENT_HANDLER(physical_ready)
{
    DATA_FRAME f;
    size_t len;
    int link;
    int checksum;
    int i;

    len = sizeof(DATA_FRAME);

    /*printf("\n\n<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>>");
    printf("\nLength of DATA_FRAME: %d", (int)len);
    printf("\n<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>>\n\n");*/

    CHECK(CNET_read_physical(&link, (char*) &f, &len));

    checksum = f.checksum;
    f.checksum  = 0;

    printf("\n====================================================================\n");
    printf("Incoming checksum: %d,\tCalculated checksum: %d", checksum, CNET_ccitt((unsigned char*) &f, len));
    printf("\n====================================================================\n");

    if(CNET_ccitt((unsigned char*) &f, len) != checksum)
    {
        // bad checksum, ignore frame
        printf("\t\t\t\tBAD checksum - frame ignored\n");
        send_ack_frame(last_ack_sent + 1, REJ, routing_table[nodeinfo.nodenumber][f.srcnode]);
        return;
    }

    if(f.type == DATA){
        if(f.destnode == nodeinfo.nodenumber){
            r_status[f.seq] = FRM_RECV;
            printf("\t\t\t\tDATA received, seq=%d, From Node Number: %d\n", f.seq, f.srcnode);
            //Run through the received buffer to find the last frame that this node received
            //where it breaks the next sequence number to expect
            for(i = (last_ack_sent + 1) % (MAXSEQ + 1); i != f.seq; i = (i + 1) % (MAXSEQ + 1))
            {
                if(r_status[i] != FRM_RECV)
                    break;
            }
            if(i == f.seq)
                send_ack_frame(f.seq, RR, routing_table[nodeinfo.nodenumber][f.srcnode]);
        }
        else
            middle_man_send(f, f.destnode);
    }
    else{
        if(f.seq > 0)
        {
            // RR FRAME
            printf("\t\t\t\tRR received, seq=%d, From Node Number: %d\n", f.seq, f.srcnode);

            int i, c = 0;
            for(i = (last_ack_recv + 1)%(MAXSEQ + 1); i != f.seq - 1; i = (i + 1) % (MAXSEQ + 1))
            {
                t_status[i] = ACK_RECV;
                c++;
            }
            last_ack_recv = f.seq - 1;

            t_status[f.seq-1] = ACK_RECV;
            CNET_stop_timer(wait_timer);

            CNET_enable_application(ALLNODES);
        }
        else
        {
            // REJ FRAME
            f.seq = -f.seq;
            printf("\t\t\t\tREJ received, seq=%d, From Node Number: %d\n", f.seq, f.srcnode);
            CNET_stop_timer(timer);
            int i;
            for(i = f.seq; i != next_frame; i = (i + 1) % (MAXSEQ + 1))
            {
                t_status[i] = ACK_WAIT;
                send_data_frame(&(t_window[i].msg), t_window[i].len, i, f.srcnode);
            }
        }
    }
}

static EVENT_HANDLER(timer_timeout)
{
    int i;

    printf("Timeout. Resending\n");
    // Acknowledgement not received. Resend from last unacknowledged frame
    for(i = (last_ack_recv + 1) % (MAXSEQ + 1); i != next_frame; i = (i + 1) % (MAXSEQ + 1))
        send_data_frame(&(t_window[i].msg), t_window[i].len, i, t_window[i].destnode);
    send_data_frame(&(t_window[next_frame].msg), t_window[next_frame].len, i, t_window[next_frame].destnode);
}

EVENT_HANDLER(reboot_node)
{
    int i;

    printf("I am Node: %d\n", nodeinfo.nodenumber);
    printf("Number of Nodes: %d\n", NNODES);
    printf("Number of Node Links: %d\n", nodeinfo.nlinks);

    last_msg = calloc(1, MAX_MESSAGE_SIZE);
    wait_timer = NULLTIMER;

    for(i=0; i<=MAXSEQ; i++)
        t_status[i] = ACK_RST;

    for(i=0; i<=MAXSEQ; i++)
        r_status[i] = FRM_RST;

    CHECK(CNET_set_handler(EV_APPLICATIONREADY, app_ready, 0));
    CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
    CHECK(CNET_set_handler(EV_TIMER1, timer_timeout, 0));
    CNET_enable_application(ALLNODES);
}
