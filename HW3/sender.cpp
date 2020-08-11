#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include "opencv2/opencv.hpp"
#include <iostream>

#include "common.h"

using namespace std;
using namespace cv;

int main(int argc, char *argv[])
{
    int sockfd; 
    char buffer[MAXBUFF]; 
    segment SND, RCV;

    //Things for the select
    fd_set readfds, master;
    struct timeval timeout;

    FD_ZERO(&master);
    FD_ZERO(&readfds);

    struct sockaddr_in sender_addr, agent_addr; 
    init_header(&SND.head, 0);
    init_header(&RCV.head, 0);

    char ip_addr[20];
    char fname[256];
    int portA, portS;

    if (argc != 5) {
        fprintf(stderr, "Format: %s <Agent IP> <Agent port> <Sender Port> <File name>\n", argv[0]);
        fprintf(stderr, "For example: ./sender, local 8888 8887 tmp.mpg\n");
        exit(0);
    }
    else {
        setIP(ip_addr, argv[1]);
        strcpy(fname, argv[4]);

        if (sscanf(argv[2], "%d", &portA) < 0) {
            perror("Incorrect agent port");
            exit(0);
        }
        if (sscanf(argv[3], "%d", &portS) < 0) {
            perror("Incorrect sender port");
            exit(0);
        }

    }
    memset(&sender_addr, 0, sizeof(sender_addr));  
    memset(&agent_addr, 0, sizeof(agent_addr)); 

    //Filling sender information
    sender_addr.sin_family = AF_INET; 
    sender_addr.sin_port = htons(portS); 
    sender_addr.sin_addr.s_addr = htonl(INADDR_ANY); 

    // Filling agent information 
    agent_addr.sin_family = AF_INET; 
    agent_addr.sin_port = htons(portA); 
    agent_addr.sin_addr.s_addr = inet_addr(ip_addr); 

    socklen_t agent_size = sizeof(agent_addr);
      
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    }
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&sender_addr,  
            sizeof(sender_addr)) < 0 ) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
    
    FD_SET(sockfd, &master);  
    int fdmax = sockfd;  
    FILE *fp;
    int confirm_seq, check, last_sent;
    int window_size = 1;
    int threshold = 16;     //Initialize control variables
    last_sent = 0;

    unsigned int acked, last, result, sent_last;
    acked = 0;
    last = -1;
    sent_last = 0;

    if ((fp = fopen(fname, "r")) != NULL) {
        int flag = 0;
        while (1) {
            int max_window, min_window;
            min_window = acked + 1;
            max_window = acked + window_size;
            for (int i = min_window; i <= max_window && !flag; i++) {
                fseek(fp, (i-1)*MAXBUFF, SEEK_SET);
                //Send an entire window of packages
                if ((result = fread(SND.data, 1, MAXBUFF, fp)) < 0) {
                    perror("Error reading data");
                    exit(0);
                }
                if (result == 0) {
                    last = i-1;
                    break;
                }
                SND.head.length = result;
                SND.head.seqNumber = i;
                int tmp;
                if ((tmp = sendto(sockfd, &SND, sizeof(SND), 0, (struct sockaddr *)&agent_addr, sizeof(agent_addr))) < 0) {
                    perror("Error sending package");
                    exit(0);
                } 
                if (i > sent_last) {
                    fprintf(stderr, "send\tdata\t#%d,\twinSize = %d\n", i, window_size);
                }
                else {
                    fprintf(stderr, "resnd\tdata\t#%d,\twinSize = %d\n", i, window_size);
                }
            }

            sent_last = (max_window > sent_last) ? max_window: sent_last;
            int prev_win_size = window_size;
            //Increase of window
            if (window_size < threshold)
                window_size = 2*window_size;
            else
                window_size++;
            int recv = 0;

            while (acked < max_window) {
                if (acked == last && last != -1) {
                    flag = 1;
                    SND.head.fin = 1;
                    if (sendto(sockfd, &SND, sizeof(SND), 0, (struct sockaddr *)&agent_addr, sizeof(agent_addr)) < 0) {
                        perror("Error sending package");
                        exit(0);
                    }
                    fprintf(stderr, "send\tfin\n");
                    if (recvfrom(sockfd, &RCV, sizeof(RCV), 0, (struct sockaddr *)&agent_addr, &agent_size) < 0) {
                        perror("Error receiving package");
                        exit(0);
                    }
                    fprintf(stderr, "recv\tfinack\n");
                    break;
                }
                readfds = master;
                //Wait for all of the data, to choose if timeout or packet loss
				timeout.tv_sec = 0;
                timeout.tv_usec = 10000;
                check = select(fdmax + 1, &readfds, NULL, NULL, &timeout);
                if (check == -1) {
                    perror("Select error ");
                    exit(0);
                }
                else {
                    if (check == 0) {
                        //TODO: timeout
                        threshold = (prev_win_size/2 > 1) ? prev_win_size/2: 1;
                        window_size = 1;
                        fprintf(stderr, "time\tout,\t\tthreshold = %d\n", threshold);
                        break;
                    }
                    else {
                        //Each time an ack is received, forward ack_fp by those many bytes in length
                        //For go back n, if this fails, next time it starts reading from ack_fp
                        if (recvfrom(sockfd, &RCV, sizeof(RCV), 0, (struct sockaddr *)&agent_addr, &agent_size) < 0) {
                            perror("Error receiving package");
                            exit(0);
                        }
                        if (RCV.head.ack == 1) {
                            fprintf(stderr, "recv\tack\t#%d\n", RCV.head.ackNumber);
                            if (RCV.head.ackNumber == acked+1)
                                acked++;
                        }
                    }
                }
            }
            if (flag)
                break;
        }
    }
    else {
        perror("File not found\n");
        exit(0);
    }

    fclose(fp);
    return 0;
}
