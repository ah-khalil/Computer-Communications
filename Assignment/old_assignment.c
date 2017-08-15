#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include "assignment.h"

static MSG *lastmsg;
static int framenum = 0;
static int ackexpected = 0;
static int frameexpected = 0;
static size_t lastlength = 0;
static int nextframetosend = 0;
static CnetTimerID	lasttimer = NULLTIMER;

static void send_frame(MSG *msg, FRAMEKIND kind, size_t length, int seqno)
{
    FRAME f;
    int link = 1;
    CnetTime timeout;

    f.seq = seqno;
    f.len = length;
    f.kind = kind;
    f.framenum = framenum;
    f.nodenum = nodeinfo.nodenumber;
    f.checksum = 0;

    framenum++;

    switch (kind) {
        case DL_DATA:
            printf("This is Frame No: %d, from Node %d, Sequence No: %d\n", f.framenum, f.nodenum, f.seq);
            memcpy(&f.msg, (char *)msg, (int)length);
	        timeout = FRAME_SIZE(f)*((CnetTime)8000000 / linkinfo[link].bandwidth) + linkinfo[link].propagationdelay;
            lasttimer = CNET_start_timer(EV_TIMER1, 3 * timeout, 0);
	    break;

        case DL_ACK :
            printf("\nACK transmitted, Sequence No: %d\n", seqno);
	    break;

        case DL_REJ :
        break;
    }

    length = FRAME_SIZE(f);
    f.checksum = CNET_ccitt((unsigned char *)&f, (int)length);

    printf("Checksum: %d\n\n", f.checksum);

    CHECK(CNET_write_physical(link, (char *)&f, &length));
}

static EVENT_HANDLER(application_ready)
{
    CnetAddr destaddr;

    lastlength  = sizeof(MSG);
    CHECK(CNET_read_application(&destaddr, (char *)lastmsg, &lastlength));
    CNET_disable_application(ALLNODES);
    send_frame(lastmsg, DL_DATA, lastlength, nextframetosend);
    nextframetosend = 1 - nextframetosend;
}

static EVENT_HANDLER(physical_ready)
{
    FRAME f;
    size_t len;
    int link, checksum;

    len = sizeof(FRAME);
    CHECK(CNET_read_physical(&link, (char *)&f, &len));

    checksum = f.checksum;
    f.checksum = 0;

    printf("Sent Checksum = %d, Calculated Checksum = %d\n\n", checksum, CNET_ccitt((unsigned char*) &f, len));
    if(CNET_ccitt((unsigned char *)&f, (int)len) != checksum) {
        printf("\t\t\t\tBAD checksum - frame ignored\n");
        return;
    }

    switch (f.kind) {
        case DL_DATA :
            printf("This is Frame No: %d, from Node %d, Sequence No: %d, Checksum: %d\n", f.framenum, f.nodenum, f.seq, checksum);
            if(f.seq == frameexpected) {
                len = f.len;
                CHECK(CNET_write_application((char *)&f.msg, &len));
                frameexpected = 1 - frameexpected;
            }
            else
                printf("\n\t\t\t***********Data ignored***********\n");
            send_frame(NULL, DL_ACK, 0, f.seq);
        break;

        case DL_ACK :
            if(f.seq == ackexpected) {
                printf("\n\t=====Acknowledgement received, from Node %d, Sequence No: %d=====\n\n", f.nodenum, f.seq);
                CNET_stop_timer(lasttimer);
                ackexpected = 1 - ackexpected;
                CNET_enable_application(ALLNODES);
            }
    	break;

        case DL_REJ :
        break;
    }
}

static EVENT_HANDLER(timeouts)
{
    printf("Timeout, Sequence No: %d\n", ackexpected);
    send_frame(lastmsg, DL_DATA, lastlength, ackexpected);
}

static EVENT_HANDLER(showstate)
{
    printf("\n\t\tAcknowledgement Expected: %d\n\t\tNext Frame To Send: %d\n\t\tFrame Expected: %d\n\n", ackexpected, nextframetosend, frameexpected);
}

EVENT_HANDLER(reboot_node)
{
    lastmsg	= calloc(1, sizeof(MSG));

    CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
    CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
    CHECK(CNET_set_handler(EV_TIMER1, timeouts, 0));
    CHECK(CNET_set_handler(EV_DEBUG0, showstate, 0));

    CHECK(CNET_set_debug_string(EV_DEBUG0, "State"));

	CNET_enable_application(ALLNODES);
}
