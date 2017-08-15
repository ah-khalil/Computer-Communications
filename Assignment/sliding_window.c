#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Regular Frame Size = 512 bits = 64 bytes
#define DATA_FRAME_SIZE_MAX 64

// Data Frame Header consists of checksum and sequence number
#define DATA_FRAME_H_SIZE   (sizeof(uint16_t) + sizeof(int) + sizeof(size_t))

#define DATA_SIZE_MAX       (DATA_FRAME_SIZE_MAX - DATA_FRAME_H_SIZE)

// Ack Frame Size = 48 bits = 6 bytes
#define ACK_FRAME_SIZE      6

// Transmission Time of Data and Ack Frame
#define DATA_TRANS_TIME     50
#define ACK_TRANS_TIME      5

#define TIMEOUT     3600
#define CHECK_BYTES 8
#define MAXSEQ      10
#define ACK_RST     0
#define ACK_RECV    1
#define ACK_WAIT    2
#define RR          1
#define REJ         2
#define DATA        3
#define FRM_RECV    1
#define FRM_RST     0

typedef struct {
    char data[DATA_SIZE_MAX];
} MSG;

typedef struct {
    int seq;            // Seq Number of the Frame
    size_t len;
    int checksum;       // Checksum of the frame (first 64 bits)
    int type;
    MSG msg;            // Message sent by the transmitter
} DATA_FRAME;

#define DATA_FRAME_SIZE(f)  (DATA_FRAME_H_SIZE + f.len)

static int         last_ack_recv = -1;  // Seq Number of last ack frame recvd
static int         last_ack_sent = -1;  // Seq Number of last ack frame sent
static MSG         *last_msg;
static CnetTimerID wait_timer = NULLTIMER;

static int next_frame = 0;     // Next Frame to be sent
static int packets_sent = 0;
static int t_status[MAXSEQ + 1];    // Status of Frames in Transmitter Window
static int r_status[MAXSEQ + 1];    // Status of Frames in Receiver Window
static DATA_FRAME t_window[MAXSEQ + 1];    // Transmitter Window

static void send_data_frame(MSG *msg, size_t len, int seq);
static void send_ack_frame(int seq, int type);

static EVENT_HANDLER(app_ready)
{
    CnetAddr dest;
    size_t len;

    if(t_status[next_frame] == ACK_WAIT)
    {
        wait_timer = CNET_start_timer(EV_TIMER1, TIMEOUT, 0);
        CNET_disable_application(ALLNODES);
        return;
    }

    len = MAX_MESSAGE_SIZE;
    CHECK(CNET_read_application(&dest, (char*) last_msg, &len));
    send_data_frame(last_msg, len, next_frame);
    t_status[next_frame] = ACK_WAIT;
    next_frame = (next_frame + 1) % (MAXSEQ + 1);
}

static void send_data_frame(MSG *msg, size_t len, int seq)
{
    if(len > DATA_SIZE_MAX)
        len = DATA_SIZE_MAX;

    DATA_FRAME f;
    f.seq = seq;
    f.len = len;
    f.type = DATA;
    f.checksum = 0;
    memcpy(&f.msg, msg, len);
    t_window[seq] = f;

    size_t size = DATA_FRAME_SIZE(f);
    f.checksum  = CNET_ccitt((unsigned char *) &f, (int)size);

    printf("DATA transmitted,\tSequence No: %d,\tChecksum: %d\n", seq, f.checksum);
    CHECK(CNET_write_physical_reliable(1, (char *) &f, &size));
}

static void send_ack_frame(int seq, int type)
{
    DATA_FRAME f;
    f.checksum = 0;
    f.type = type;
    if(type == REJ){
        f.seq = -seq;
        printf("Reject transmitted,\tSequence No: %d,\t", f.seq);
    }
    else{
        f.seq = seq;
        printf("Receive-Ready transmitted,\tSequence No: %d,\t", f.seq);
    }

    size_t size = sizeof(DATA_FRAME);
    f.checksum = CNET_ccitt((unsigned char *) &f, (int)size);

    printf("Checksum: %d\n", f.checksum);

    CHECK(CNET_write_physical_reliable(1, (char *) &f, &size));
}

static EVENT_HANDLER(physical_ready)
{
    DATA_FRAME f;
    size_t len;
    int link;
    int checksum;

    len = sizeof(DATA_FRAME);
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
        send_ack_frame(last_ack_sent + 1, REJ);
        return;
    }

    if(f.type == DATA){
        r_status[f.seq] = FRM_RECV;
        printf("\t\t\t\tDATA received, seq=%d\n", f.seq);

        int i;
        for(i = (last_ack_sent + 1) % (MAXSEQ + 1); i != f.seq; i = (i + 1) % (MAXSEQ + 1))
        {
            if(r_status[i] != FRM_RECV)
                break;
        }
        if(i == f.seq)
            send_ack_frame(f.seq + 1, RR);
    }
    else{
        if(f.seq > 0)
        {
            // RR FRAME
            printf("\t\t\t\tRR received, seq=%d\n", f.seq);

            int i, c = 0;
            for(i = (last_ack_recv + 1)%(MAXSEQ + 1); i != f.seq - 1; i = (i + 1) % (MAXSEQ + 1))
            {
                t_status[i] = ACK_RECV;
                c++;
            }
            last_ack_recv = f.seq - 1;

            t_status[f.seq-1] = ACK_RECV;
            CNET_stop_timer(wait_timer);

            if(packets_sent < 10000)
            {
                packets_sent += (c+1);
                CNET_enable_application(ALLNODES);
            }
            else{}
        }
        else
        {
            // REJ FRAME
            f.seq = -f.seq;
            printf("\t\t\t\tREJ received, seq=%d\n", f.seq);
            CNET_stop_timer(timer);
            int i;
            for(i = f.seq; i != next_frame; i = (i + 1) % (MAXSEQ + 1))
            {
                t_status[i] = ACK_WAIT;
                send_data_frame(&(t_window[i].msg), t_window[i].len, i);
            }
        }
    }
}

static EVENT_HANDLER(wait)
{
    int i;

    printf("Timeout. Resending\n");
    // Acknowledgement not received. Resend from last unacknowledged frame
    for(i = (last_ack_recv + 1) % (MAXSEQ + 1); i != next_frame; i = (i + 1) % (MAXSEQ + 1))
        send_data_frame(&(t_window[i].msg), t_window[i].len, i);
    send_data_frame(&(t_window[next_frame].msg), t_window[next_frame].len, i);
}

EVENT_HANDLER(reboot_node)
{
    int i;

    last_msg = calloc(1, MAX_MESSAGE_SIZE);
    wait_timer = NULLTIMER;

    for(i=0; i<=MAXSEQ; i++)
        t_status[i] = ACK_RST;

    for(i=0; i<=MAXSEQ; i++)
        r_status[i] = FRM_RST;

    CHECK(CNET_set_handler(EV_APPLICATIONREADY, app_ready, 0));
    CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
    CHECK(CNET_set_handler(EV_TIMER1, wait, 0));
    CNET_enable_application(ALLNODES);
}
