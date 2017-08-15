#include <cnet.h>
void reboot_node(CnetEvent ev, CnetTimerID timer, CnetData data){
    printf("Hello World\n");
    printf("I am node %d\n", nodeinfo.nodenumber);
    if(nodeinfo.nlinks > 0)
        printf("There are %d link(s) connecting me to the outside world\n", nodeinfo.nlinks);
    else
        printf("There are no nodes\n");
}
