typedef struct Packet{
    int s_address;
    int r_address;
    char* payload;
} Packet;

typedef struct Frame{
    int seth_address;
    int reth_address;
    Packet* payload;
} Frame;

void giveData();
void downFromN3(char* msg);
void downFromN2(Packet* pkt);
void Phy(Frame* f);
void upFromPhy(Frame* f);
void upFromN1(Packet* pkt);
void upFromN2(char* msg);
