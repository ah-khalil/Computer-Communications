#define FRAME_SIZE_MAX 64
#define TIMEOUT     3600
#define MAX_SEQ     10
#define ACK_RST     0
#define ACK_RECV    1
#define ACK_WAIT    2
#define RR          1
#define REJ         2
#define DATA        3
#define FRM_RECV    1
#define FRM_RST     0

typedef struct {
    char data[MAX_MESSAGE_SIZE];
} MSG;

typedef struct {
    int seq;                                // Seq Number of the Frame
    size_t len;                             // Length of the message
    int checksum;                           // Checksum of the frame
    int type;                               // What type of frame it is (DATA, RR, REJ)
    int src_node;                           // Source node of the frame
    int dest_node;                          // Destination node of the frame
    MSG msg;                                // Message sent by the transmitter
} FRAME;

#define FRAME_HEADER_SIZE   (sizeof(FRAME) - sizeof(MSG))
#define DATA_SIZE_MAX       (FRAME_SIZE_MAX - FRAME_HEADER_SIZE)
#define FRAME_SIZE(f)  (FRAME_HEADER_SIZE + f.len)

static int         last_ack_recv = -1;      // Seq Number of last ack frame received
static int         last_ack_sent = -1;      // Seq Number of last ack frame sent
static MSG         *last_msg;
static CnetTimerID wait_timer = NULLTIMER;

static int next_frame = 0;                  // Next Frame to be sent
static int t_status[MAX_SEQ + 1];           // Frames in Transmitter Window
static int r_status[MAX_SEQ + 1];           // Status of Frames in Receiver Window
static int routing_table[5][5] = {{0, 1, 2, 2, 2}, {1, 0, 2, 2, 2}, {1, 2, 0, 3, 3}, {1, 1, 1, 0, 2}, {1, 1, 1, 1, 0}};
static FRAME t_window[MAX_SEQ + 1];     // Transmitter Window

static void send_data_frame(MSG *msg, size_t len, int seq, int dest);
static void send_ack_frame(int seq, int type, int link);
