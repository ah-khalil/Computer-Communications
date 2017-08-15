#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf_struct.h"

void giveData(){
    char* msg = (char*)malloc(40* sizeof(char));

    strcpy(msg, "This is a message");

    downFromN3(msg);
}

void downFromN3(char* msg){
    Packet* pk = (Packet*)malloc(sizeof(Packet));
    srand(time(NULL));

    pk->s_address = rand();
    pk->r_address = rand();
    pk->payload = msg;

    downFromN2(pk);
}

void downFromN2(Packet* pkt){
    Frame* fr = (Frame*)malloc(sizeof(Frame));

    fr->seth_address = rand();
    fr->reth_address = rand();
    fr->payload = pkt;

    Phy(fr);
}

void Phy(Frame* f){
    upFromPhy(f);
}

void upFromPhy(Frame* fr){
    printf("Sender Ethernet Address: %d\nReciever Ethernet Address: %d\n", fr->seth_address, fr->reth_address);

    upFromN1(fr->payload);
}

void upFromN1(Packet* pkt){
    printf("Sender Address: %d\nReciever Address: %d\n", pkt->s_address, pkt->r_address);

    upFromN2(pkt->payload);
}

void upFromN2(char* msg){
    printf("Message: %s\n", msg);
}
