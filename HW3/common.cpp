#include <iostream>
#include <errno.h>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "opencv2/opencv.hpp"
#include "common.h"

// #define MAXBUFF 1000


void init_header(header *src, int ack) {
	src->length = 0;
    src->seqNumber = 0;
    src->ackNumber = 0;
    src->fin = 0;
	src->syn = 0;
	src->ack = ack;
	return;
}

void setIP(char *dst, char *src) {
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0
            || strcmp(src, "localhost")) {
        sscanf("127.0.0.1", "%s", dst);
    } else {
        sscanf(src, "%s", dst);
    }
}