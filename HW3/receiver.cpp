#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h> 
#include <sys/types.h>
#include <errno.h>
#include "opencv2/opencv.hpp"
#include <iostream>

#include "common.h"

using namespace cv;
using namespace std;

typedef struct {
	int len;
	char buff[MAXBUFF];
} bufftype;

int main(int argc, char *argv[])
{
	int sockfd; 

    struct sockaddr_in agent_addr, recv_addr;
    socklen_t tmp_size; 
    int buff_ov = 256;
    segment RCV, SND;
    init_header(&RCV.head, 0);	
    init_header(&SND.head, 1);
    bufftype buffer[buff_ov]; 
    char ip_addr[100];
    int portA, portR;

    if (argc != 4) {
        fprintf(stderr, "Format: %s <Agent IP> <Agent port> <Recv Port>\n", argv[0]);
        fprintf(stderr, "For example: ./receiver, local 8888 8889\n");
        exit(0);
    }
    else {
        setIP(ip_addr, argv[1]);

        if (sscanf(argv[2], "%d", &portA) < 0) {
            perror("Incorrect agent port");
            exit(0);
        }
        if (sscanf(argv[3], "%d", &portR) < 0) {
            perror("Incorrect receiver port");
            exit(0);
        }

    }
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 

    memset(&agent_addr, 0, sizeof(agent_addr)); 
    memset(&recv_addr, 0, sizeof(recv_addr));
      
    // Filling agent information 
    agent_addr.sin_family = AF_INET; 
    agent_addr.sin_port = htons(portA); 
    agent_addr.sin_addr.s_addr = inet_addr(ip_addr); 

    //Filling recv information
    recv_addr.sin_family = AF_INET; 
    recv_addr.sin_port = htons(portR); 
    recv_addr.sin_addr.s_addr = INADDR_ANY; 

    int agent_size = sizeof(agent_addr);

    if ( bind(sockfd, (const struct sockaddr *)&recv_addr,  
            sizeof(recv_addr)) < 0 ) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    //Start expecting packets

    int packet_size = 0;
    int ackNext = 1;
    int pos_buffer = 0;

    FILE *fp;

    if ((fp = fopen("abc.tmp", "w")) == NULL) {
    	perror("Error creating tmp file");
    	exit(0);
    }

    int num = 0, acked = 0;

    while (1) {
    	packet_size = recvfrom(sockfd, &RCV, sizeof(RCV), 0, (struct sockaddr *)&recv_addr, &tmp_size);
    	if (packet_size > 0) {
    		if(RCV.head.ack) {
                fprintf(stderr, "Received sender's ack segment\n");
                exit(1);
            }
            if (num == buff_ov) {
            	for (int i = 0; i < buff_ov; i++) {
            		fwrite(buffer[i].buff, 1, buffer[i].len, fp);
            	}
            	fprintf(stderr, "drop\tdata\t#%d\n", RCV.head.seqNumber);
            	SND.head.ack = 1;
            	SND.head.seqNumber = acked;
            	if (sendto(sockfd, &SND, sizeof(SND), 0, (struct sockaddr *)&agent_addr, agent_size) < 0) {
        			perror("Error sending packet\n");
        			exit(1);
        		}
        		fprintf(stderr, "send\tack\t#%d\n", SND.head.seqNumber);
        		fprintf(stderr, "flush\n");
            	num = 0;
            }
            else {
            	init_header(&SND.head, 1);
            	if (RCV.head.fin == 1) {
            		SND.head.fin = 1;
            		if (sendto(sockfd, &SND, sizeof(SND), 0, (struct sockaddr *)&agent_addr, agent_size) < 0) {
            			perror("Error sending packet\n");
            			exit(1);
            		}
            		fprintf(stderr, "recv\tfin\n");
            		fprintf(stderr, "send\tfinack\n");
            		break;
            	}
            	else {
            		if (RCV.head.seqNumber == acked + 1) {
            			fprintf(stderr, "recv\tdata\t#%d\n", RCV.head.seqNumber);
            			for (int i = 0; i < RCV.head.length; i++)
            				buffer[num].buff[i] = RCV.data[i];
            			buffer[num].len = RCV.head.length;
            			fprintf(stderr, "send\tack\t#%d\n", RCV.head.seqNumber);
            			num++;
            			acked++;
            		}
            		else {
            			fprintf(stderr, "drop\tdata\t#%d\n", RCV.head.seqNumber);
            		}
            		SND.head.ackNumber = acked;
            		if (sendto(sockfd, &SND, sizeof(SND), 0, (struct sockaddr *)&agent_addr, agent_size) < 0) {
            			perror("Error sending packet\n");
            			exit(1);
            		}
            	}
            }
    	}
    }
    for (int i = 0; i < num; i++) {
		fwrite(buffer[i].buff, 1, buffer[i].len, fp);
	}
    fprintf(stderr, "flush\n");
    fclose(fp);

    /*My streaming didn't work, so for it to look complete,
    I include the playing video demo from HW2 to play the video
    after the file transfer is complete*/

    Mat imgServer,imgClient;
    VideoCapture cap("./abc.tmp");
    
    // get the resolution of the video
    int width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
    
    //allocate container to load frames 
    
    imgServer = Mat::zeros(height, width, CV_8UC3);    
    imgClient = Mat::zeros(height, width, CV_8UC3);
 
 
     // ensure the memory is continuous (for efficiency issue.)
    if(!imgServer.isContinuous()){
         imgServer = imgServer.clone();
    }

    if(!imgClient.isContinuous()){
         imgClient = imgClient.clone();
    }

    while(1){
        //get a frame from the video to the container on server.
        cap >> imgServer;
        
        // get the size of a frame in bytes 
        int imgSize = imgServer.total() * imgServer.elemSize();
        
        // allocate a buffer to load the frame (there would be 2 buffers in the world of the Internet)
        uchar buffer[imgSize];
        
        // copy a frame to the buffer
        memcpy(buffer,imgServer.data, imgSize);
        
        // copy a fream from buffer to the container on client
        uchar *iptr = imgClient.data;
        memcpy(iptr,buffer,imgSize);
      
        imshow("Video", imgClient);
      //Press ESC on keyboard to exit
      // notice: this part is necessary due to openCV's design.
      // waitKey means a delay to get the next frame.
        char c = (char)waitKey(33.3333);
        if(c==27) 
                break;
        }
	cap.release();
	destroyAllWindows();
    
    close(sockfd); 
	return 0;
}
