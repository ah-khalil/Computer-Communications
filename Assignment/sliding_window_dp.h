// Regular Frame Size = 512 bits = 64 bytes
#define DATA_FRAME_SIZE_MAX 64
// Data Frame Header consists of checksum and sequence number
#define DATA_FRAME_H_SIZE   (sizeof(uint16_t) + sizeof(int) + sizeof(size_t))

#define DATA_SIZE_MAX       (DATA_FRAME_SIZE_MAX - DATA_FRAME_H_SIZE)

#define TIMEOUT     3600
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
    int seq;                                // Seq Number of the Frame
    size_t len;                             // Length of the message
    int checksum;                           // Checksum of the frame (first 64 bits)
    int type;                               // What type of frame it is (DATA, RR, REJ)
    int srcnode;                            // Source node of the frame
    int destnode;                           // Destination node of the frame
    int link;                               // Link number through which the message will be transmitted
    MSG msg;                                // Message sent by the transmitter
} DATA_FRAME;

#define DATA_FRAME_SIZE(f)  (DATA_FRAME_H_SIZE + f.len)

static int         last_ack_recv = -1;      // Seq Number of last ack frame received
static int         last_ack_sent = -1;      // Seq Number of last ack frame sent
static MSG         *last_msg;
static CnetTimerID wait_timer = NULLTIMER;

static int next_frame = 0;                  // Next Frame to be sent
static int packets_sent = 0;
static int t_status[MAXSEQ + 1];            // Frames in Transmitter Window
static int r_status[MAXSEQ + 1];            // Status of Frames in Receiver Window
static int routing_table[3][3] = {{0, 1, 1}, {1, 0, 2}, {1, 1, 0}};
static DATA_FRAME t_window[MAXSEQ + 1];     // Transmitter Window

static void send_data_frame(MSG *msg, size_t len, int seq, int dest);
static void send_ack_frame(int seq, int type, int link);
