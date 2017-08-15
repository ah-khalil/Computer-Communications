typedef enum { DL_DATA, DL_ACK, DL_REJ }   FRAMEKIND;

typedef struct {
    char data[MAX_MESSAGE_SIZE];
} MSG;

typedef struct {
    FRAMEKIND kind;
    size_t len;
    int checksum;
    int seq;
    int framenum;
    int nodenum;
    MSG msg;
} FRAME;

#define FRAME_HEADER_SIZE  (sizeof(FRAME) - sizeof(MSG))
#define FRAME_SIZE(f)      (FRAME_HEADER_SIZE + f.len)
#define WINDOW_MAX_SIZE 7
