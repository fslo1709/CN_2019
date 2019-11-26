#include <iostream>
#include <stdio.h>
#include <dirent.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "opencv2/opencv.hpp"

#include "common.h"

#define BUFF_SIZE 1024
#define FLEN 300

using namespace std;
using namespace cv;

char dir_name[30];

int my_ls(char *pathname, int s) {
    DIR *dp;
    struct dirent *dirp;
    char buf[BUFF_SIZE] = {};
    int s_err, len;
    if ((dp = opendir(pathname)) == NULL) {
        return -1;
    }
    while ((dirp = readdir(dp)) != NULL) {
        char temp[260];
        strcpy(temp, dirp->d_name);
        strcat(temp, " ");
        if (strlen(temp) + strlen(buf) >= 1024) {
            len = strlen(buf);
            s_err = sendall(s, buf, &len);
            if (s_err < 0) {
                return -1;                
            }
            bzero(buf, sizeof(char)*BUFF_SIZE);
            strcpy(buf, temp);
        }
        else {
            strcat(buf, temp);
        }
    }
    len = strlen(buf);
    s_err = sendall(s, buf, &len);
    if (s_err < 0) 
        return -1;
    bzero(buf, sizeof(char)*BUFF_SIZE);
    strcpy(buf, "\n");
    len = strlen(buf);
    s_err = sendall(s, buf, &len);
    if (s_err < 0)
        return -1;
    closedir(dp);
    return 0;
}

int my_put(char *name, int s) {
    char size[FLEN] = {}, fname[FLEN] = {};
    FILE *fp;
    int i = 0, sent, len;
    char ok_mes[4], nk_mes[4];
    sprintf(ok_mes, "OK");
    sprintf(nk_mes, "NK");
    while (name[i] != ' ' and name[i] != '\0') {
        size[i] = name[i];
        i++;
    }
    if (name[i] == ' ') {
        i++;
        long long int numsize;
        sscanf(size, "%lld", &numsize);
        strcpy(fname, name + i);    //Get the name of the file to copy
        char actual_name[FLEN] = {};
        sprintf(actual_name, "myserv_folder/%s", fname);
        if ((fp = fopen(actual_name, "wb"))!= NULL) {
            len = strlen(ok_mes);
            sent = sendall(s, ok_mes, &len);
            if (sent < 0)
                return -1;
            int rec_err = my_receive(s, fp, numsize);
            if (rec_err < 0)
                fprintf(stderr, "Error receiving file\n");
            fclose(fp);
        }
        else {
            len = strlen(nk_mes);
            sent = sendall(s, nk_mes, &len);
            if (sent < 0)
                return -1;
        }
    }
    else {
        fprintf(stderr, "Unexpected error\n");
        return -1;
    }
    return 0;
}

int my_get(char *buf, int s) {
    char fname[FLEN] = {};
    char ok_mes[FLEN], nf_mes[4];
    char receiveMessage[BUFF_SIZE] = {};
    sprintf(nf_mes, "NF");
    sprintf(fname, "myserv_folder/%s", buf);
    FILE *fp;
    int sent, len, flag = 0;
    if ((fp = fopen(fname, "rb")) != NULL) {
        fseek(fp, 0, SEEK_END);
        long long int len_file = ftell(fp);
        rewind(fp);
        sprintf(ok_mes, "%lld", len_file);
        len = strlen(ok_mes);
        sent = sendall(s, ok_mes, &len);        //File exists
        if (sent < 0)
            flag = -1;
        else {
            int rec_ok = recv(s, receiveMessage, sizeof(char)*BUFF_SIZE, 0);
            if (rec_ok <= 0) {
                flag = -1;
            }
            else {
                if (strcmp(receiveMessage, "OK") == 0) {
                    int transfer_file = my_transfer(s, fp, len_file);
                    if (transfer_file < 0)
                        flag = -1;
                }
                else {
                    flag = -1;
                    fprintf(stderr, "%s\n", fname);
                }
            }
        }
        if (fclose(fp) == EOF) {
            flag = -1;
            fprintf(stderr, "Error closing file\n");
        }
    }
    else {
        len = strlen(nf_mes);
        sent = sendall(s, nf_mes, &len);        //File DNE
        if (sent < 0)
            flag = -1;
    }
    return flag;
}

int my_stream(int s, char *fname) {
    char sendMessage[FLEN] = {};
    char receiveMessage[5] = {};
    int flag = 0;
    Mat imgServer;
    VideoCapture cap(fname);
    int width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
    sprintf(sendMessage, "%d %d\n", height, width);
    int len = strlen(sendMessage);
    int sent = sendall(s, sendMessage, &len);
    imgServer = Mat::zeros(height, width, CV_8UC3); 
    if(!imgServer.isContinuous()){
         imgServer = imgServer.clone();
    }
    int rec_err = rec_tilln(s, receiveMessage);
    if (rec_err < 0) {
        flag = -1;
    } 
    else {  //Good to go, start streaming
        while (1) {
            bzero(sendMessage, sizeof(char)*FLEN);
            bzero(receiveMessage, sizeof(char)*FLEN);
            cap >> imgServer;
            int imgSize = imgServer.total() * imgServer.elemSize();
            sprintf(sendMessage, "%d\n", imgSize);
            len = strlen(sendMessage);
            sent = sendall(s, sendMessage, &len);
            if (sent < 0) {
                flag = -1;
                break;
            }
            else {
                uchar buffer[imgSize];
                memcpy(buffer, imgServer.data, imgSize);
                rec_err = rec_tilln(s, receiveMessage); //Receive ok message
                if (rec_err < 0 || strncmp(receiveMessage, "NK", 2) == 0) {
                    flag = -1;
                    break;
                }
                else {
                    len = imgSize;
                    sent = senduall(s, buffer, &len);
                    if (sent < 0) {
                        flag = -1;
                        break;
                    }
                    else {
                        rec_err = rec_tilln(s, receiveMessage);
                        if (rec_err < 0 || strncmp(receiveMessage, "NK", 2) == 0) {
                            flag = -1;
                            break;
                        }
                        else {
                            if (strncmp(receiveMessage, "EF", 2) == 0)
                                break;
                        }
                    }
                }
            }
        }
        cap.release();
    }
    return flag;
}

int my_play(char *buf, int s) {
    char fname[FLEN] = {};
    char nf_mes[4], wf_mes[FLEN], ok_mes[FLEN];
    sprintf(fname, "myserv_folder/%s", buf);
    sprintf(nf_mes, "NF");
    sprintf(wf_mes, "WF");
    FILE *fp;
    int len, sent, flag = 0;
    if ((fp = fopen(fname, "rb")) != NULL) {
        if (strlen(fname) < 4) {
            flag = -1;
        }
        else {
            char *extension, *pt;
            extension = &fname[strlen(fname) - 4];
            pt = strchr(fname, '.');
            if (strcmp(extension, ".mpg") == 0 && extension == pt) {
                fseek(fp, 0, SEEK_END);
                long long int len_file = ftell(fp);
                rewind(fp);
                sprintf(ok_mes, "%lld", len_file);
                len = strlen(ok_mes);
                sent = sendall(s, ok_mes, &len);
                if (sent < 0)
                    flag = -1;
                else {
                    int str = my_stream(s, fname);
                    if (str < 0)
                        flag = -1;
                }
            }
            else {
                len = strlen(wf_mes);
                sent = sendall(s, wf_mes, &len);
                if (sent < 0)
                    flag = -1;
            }
        }
        if (fclose(fp) == EOF) {
            fprintf(stderr, "Error closing file\n");
            flag = -1;
        }
    }
    else {
        len = strlen(nf_mes);
        sent = sendall(s, nf_mes, &len);
        if (sent < 0)
            flag = -1;
    }
    return flag;
}
fd_set master;
fd_set readfds;
int fdmax;
int localSocket, remoteSocket;

void closeSockets(int sig) {
    readfds = master;
    char buf[3] = {};
    sprintf(buf, "XD");
    for (int i = 0; i <= fdmax; i++) {
        if (i > 2 && i != localSocket) {
            if (FD_ISSET(i, &readfds)) {
                int sent = send(i, buf, strlen(buf)*sizeof(char), 0);
            }
        }
    }
    exit(0);
}

int main(int argc, char** argv){
    signal(SIGPIPE, SIG_IGN);
    //Ctrl-C Handler
    struct sigaction quitprogram;
    quitprogram.sa_handler = closeSockets;
    sigaction(SIGINT, &quitprogram, NULL);
    sigaction(SIGQUIT, &quitprogram, NULL);

    //Create a folder if it doesn't exist
    strcpy(dir_name, "./myserv_folder");
    if (access(dir_name, F_OK) == -1) {
        if (mkdir(dir_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
            perror("Error making folder\n");
            exit(0);
        }
    }
    //Variables for select

    FD_ZERO(&master);
    FD_ZERO(&readfds);

    //Connection part
    int port;

    if (sscanf(argv[1], "%d", &port) < 0) {
        perror("Invalid port\n");
        exit(0);
    }

    if (port < 0) {
        perror("Invalid port\n");
        exit(0);
    }

    struct addrinfo h, *res;
    struct sockaddr_storage remote_addr;
    socklen_t addr_size;
    memset(&h, 0, sizeof(h));
    h.ai_family = AF_INET;
    h.ai_socktype = SOCK_STREAM;
    h.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, argv[1], &h, &res);

    localSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (localSocket == -1) {
        perror("Socket() call failed ");
        exit(0);
    }
    if (bind(localSocket, res->ai_addr, res->ai_addrlen) < 0) {
        perror("Bind() call failed ");
        exit(0);
    }

    char ip_add[INET_ADDRSTRLEN];
    void *ADDR;
    ADDR = &(res->ai_addr);
    inet_ntop(res->ai_family, ADDR, ip_add, sizeof(ip_add));
    
    char receiveMessage[BUFF_SIZE] = {};

    listen(localSocket, 10);

    FD_SET(localSocket, &master);
    fdmax = localSocket;
    addr_size = sizeof(remote_addr);

    fprintf(stderr, "Server is up and running. My ip is: %s\n", ip_add);

    while(1){    
        readfds = master;
        //Wait for new data ready to be received by recv
        if (select(fdmax+1, &readfds, NULL, NULL, NULL) == -1) {
            perror("Select: ");
            exit(0);
        }
        int recved;
        bzero(receiveMessage,sizeof(char)*BUFF_SIZE);
        //Iterate through available fd's
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &readfds)) {
                if (i == localSocket) { //New Connection
                    remoteSocket = accept(localSocket, (struct sockaddr *)&remote_addr, &addr_size);
                    if (remoteSocket == -1) {
                        perror("New connection error (accept) ");
                    }
                    else {
                        FD_SET(remoteSocket, &master);
                        fdmax = (remoteSocket>fdmax) ? remoteSocket: fdmax;
                        fprintf(stderr, "Connection accepted\n");
                    }
                }
                else {  //New request from client or connection ended
                    if ((recved = recv(i, receiveMessage, sizeof(char)*BUFF_SIZE, 0)) <= 0) { //Connection closed
                        if (recved == 0) {
                            fprintf(stderr, "Connection closed\n");
                        }
                        else {
                            perror("Recv error ");
                        }
                        close(i);
                        FD_CLR(i, &master);
                    }
                    else {
                        if (strcmp(receiveMessage, "0") == 0) {
                            int er_ls = my_ls(dir_name, i);
                            if (er_ls < 0) 
                                fprintf(stderr, "Error processing ls request\n");
                        }
                        else if (receiveMessage[0] == '1') {
                            int er_put = my_put(receiveMessage + 1, i);
                            if (er_put < 0) 
                                fprintf(stderr, "Error processing put request\n");
                        }
                        else if (receiveMessage[0] == '2') {
                            int er_get = my_get(receiveMessage + 1, i);
                            if (er_get < 0) 
                                fprintf(stderr, "Error processing get request\n");
                        }
                        else if (receiveMessage[0] == '3') {
                            int er_play = my_play(receiveMessage + 1, i);
                            if (er_play < 0)
                                fprintf(stderr, "Error processing play request\n");
                        }
                    }
                }
            }
        }

    }
    return 0;
}
