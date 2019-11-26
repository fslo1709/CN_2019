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
#include <signal.h>

#include "common.h"

#define BUFF_SIZE 1024
#define FLEN 300

using namespace std;
using namespace cv;
int localSocket;

void INThandler(int sig) {
    if (localSocket != 0) {
        char nk_msg[4] = {};
        sprintf(nk_msg, "NK\n");
        int sent = send(localSocket, nk_msg, sizeof(char)*strlen(nk_msg), 0);
        if (sent < 0)
            perror("Final send error ");
        fprintf(stderr, "\nClose Socket\n");
        close(localSocket);
    }
    exit(0);
}

int parse_inst(int s, char ins, char *name) {
    char send_Message[FLEN] = {};
    sprintf(send_Message, "%c%s", ins, name);
    int len = strlen(send_Message);
    int sent = sendall(s, send_Message, &len);
    if (sent < 0) {
        perror("Failed to send request ");
        return -1;
    }
    return 0;
}

int my_rep(int s) {
    char receiveMessage[FLEN] = {}, ok_msg[4] = {}, esc_msg[4] = {};
    int flag = 0;
    sprintf(ok_msg, "OK\n");
    sprintf(esc_msg, "EF\n");
    int n_rec = rec_tilln(s, receiveMessage);
    if (n_rec < 0) {
        flag = -1;
    } 
    else {
        Mat imgClient;
        int width, height;
        sscanf(receiveMessage, "%d %d", &height, &width);
        imgClient = Mat::zeros(height, width, CV_8UC3);
        if(!imgClient.isContinuous()){
            imgClient = imgClient.clone();
        }
        int len = strlen(ok_msg);
        int sent = sendall(s, ok_msg, &len);
        if (sent < 0) {
            flag = -1;
        }
        else {  //Good to go, start receiving and playing
            while (1) {
                int imgSize;
                n_rec = rec_tilln(s, receiveMessage);
                if (n_rec < 0) {
                    flag = -1;
                    break;
                }
                else {
                    sscanf(receiveMessage, "%d", &imgSize);
                    uchar buffer[imgSize];
                    len = strlen(ok_msg);
                    sent = sendall(s, ok_msg, &len);
                    if (sent < 0) {
                        flag = -1;
                        break;
                    }
                    else {
                        len = imgSize;
                        n_rec = rec_uall(s, buffer, &len);
                        if (n_rec < 0) {
                            flag = -1;
                            break;
                        }
                        else {
                            uchar *iptr = imgClient.data;
                            memcpy(iptr, buffer, imgSize);
                            imshow("Video", imgClient);
                        }
                    }
                    char c = (char)waitKey(33.3333);
                    if(c==27) {
                        len = strlen(esc_msg);
                        sent = sendall(s, esc_msg, &len);
                        if (sent < 0)
                            flag = -1;
                        break;
                    }
                    else {
                        len = strlen(ok_msg);
                        sent = sendall(s, ok_msg, &len);
                        if (sent < 0) {
                            flag = -1;
                            break;
                        }
                    }
                }
            }
            destroyAllWindows();
        }
    }
    return flag;
}

int main(int argc , char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    //Variables for select
    fd_set master;
    fd_set readfds;
    int fdmax;

    FD_ZERO(&master);
    FD_ZERO(&readfds);

    FD_SET(STDIN_FILENO, &master);

    //Ctrl-C Handler
    struct sigaction quitprogram;
    quitprogram.sa_handler = INThandler;
    sigaction(SIGINT, &quitprogram, NULL);

    char dir_name[30];
    strcpy(dir_name, "./myclient_folder");
    if (access(dir_name, F_OK) == -1) {
        if (mkdir(dir_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
            perror("Error making folder\n");
            exit(0);
        }
    }

    char entire_argv[30];
    strcpy(entire_argv, argv[1]);

    if (strchr(entire_argv, ':')==NULL) {
        perror("Invalid address format\n");
        exit(0);
    }

    int portX;
    char port_argv[10], ip_argv[20];
    char* tok_chr;

    tok_chr = strtok(entire_argv, ":");
    if (strcpy(ip_argv, tok_chr) < 0) {
        perror("Error reading address\n");
        exit(0);
    }
    tok_chr = strtok(NULL, "");
    if (strcpy(port_argv, tok_chr) < 0) {
        perror("Error reading port\n");
        exit(0);
    }

    if (sscanf(port_argv, "%d", &portX) < 0) {
        perror("Invalid port\n");
        exit(0);
    }

    if (portX < 0 || portX > 65535) {
        perror("Invalid port\n");
        exit(0);
    }

    localSocket = socket(AF_INET , SOCK_STREAM , 0);

    if (localSocket == -1){
        perror("Fail to create a socket.\n");
        return 0;
    }

    FD_SET(localSocket, &master);
    fdmax = localSocket;

    struct sockaddr_in info;
    bzero(&info,sizeof(info));

    info.sin_family = PF_INET;
    info.sin_addr.s_addr = inet_addr(ip_argv);
    info.sin_port = htons(portX);


    int err = connect(localSocket,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        perror("Connection error\n");
        return 0;
    }
    fprintf(stderr, "Enter 0 to exit\n");
    char receiveMessage[BUFF_SIZE] = {};
    char send_Message[FLEN] = {};
    char new_line[FLEN] = {}, input_cl[FLEN] = {}, fname[FLEN] = {}, 
            len_str[FLEN] = {}, dirfname[FLEN] = {};
    while(1){
        readfds = master;
        //Variable declaration
        bzero(send_Message, sizeof(char)*FLEN);
        bzero(new_line, sizeof(char)*FLEN);
        bzero(input_cl, sizeof(char)*FLEN);
        bzero(fname, sizeof(char)*FLEN);
        bzero(len_str, sizeof(char)*FLEN);
        bzero(dirfname, sizeof(char)*FLEN);
        bzero(receiveMessage, sizeof(char)*FLEN);
        int n_rec, len, c_sent;
        FILE *fp;
        fprintf(stderr, "Ready to accept a new instruction.\n");
        //TODO: use select to check stdin and server socket
        if (select(fdmax+1, &readfds, NULL, NULL, NULL) == -1) {
            perror("Select: ");
            exit(0);
        }
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &readfds) == 0) {
                if (i != localSocket) { //TODO: actually receive the Ctrl-C from server and close this file
                    fgets(new_line, FLEN, stdin);
                    //Decide which instruction to do
                    sscanf(new_line, "%s", input_cl);
                    if (strcmp(input_cl, "ls") == 0) {
                        strcpy(send_Message, "0");
                        len = strlen(send_Message);
                        c_sent = sendall(localSocket, send_Message, &len);
                        if (c_sent < 0) {
                            perror("Failed to send request ");
                            continue;
                        }
                        while (1) {
                            bzero(receiveMessage,sizeof(char)*BUFF_SIZE);
                            if ((n_rec = recv(localSocket,receiveMessage,sizeof(char)*BUFF_SIZE,0)) <= 0){
                                if (n_rec < 0)    
                                    perror("Recv failed ");
                                if (n_rec == 0)
                                    perror("Server closed ");
                                continue;
                            }
                            if (strstr(receiveMessage, "\n") != NULL) {
                                fprintf(stderr, "%s", receiveMessage);
                                break;
                            }
                            fprintf(stderr, "%s ", receiveMessage);
                        }
                        if (n_rec <= 0)
                            continue;
                    }
                    else {
                        sscanf(new_line, "%s %s", input_cl, fname);
                        if (strcmp(input_cl, "put") == 0) {
                            sprintf(dirfname, "%s/%s", dir_name, fname);
                            if ((fp = fopen(dirfname, "rb")) == NULL) {
                                fprintf(stderr, "The %s doesn't exist.\n", fname);
                                continue;
                            }
                            fseek(fp, 0, SEEK_END);
                            long long int len_file = ftell(fp);
                            rewind(fp);
                            sprintf(len_str, "%lld %s", len_file, fname);
                            parse_inst(localSocket, '1', len_str); //Send file name
                            int temp_len = 2;
                            bzero(receiveMessage, sizeof(char)*BUFF_SIZE);
                            int er_recv = rec_all(localSocket, receiveMessage, &temp_len);  //Wait for permission to transfer
                            if (er_recv >= 0) {
                                if (strcmp(receiveMessage, "OK") != 0) 
                                    fprintf(stderr, "Error on server side\n");
                                else {
                                    int trans_file = my_transfer(localSocket, fp, len_file);
                                    if (trans_file >= 0)
                                        fprintf(stderr, "File sent successfully\n");
                                }
                            }
                            if (fclose(fp) == EOF) 
                                fprintf(stderr, "Error closing file\n");
                        }
                        else {
                            if (strcmp(input_cl, "get") == 0) {
                                long long int f_len;
                                parse_inst(localSocket, '2', fname);    //Send file name
                                bzero(receiveMessage, sizeof(char)*BUFF_SIZE);
                                int er_recv = recv(localSocket, receiveMessage, sizeof(char)*BUFF_SIZE, 0); //Wait for permission to receive file 
                                if (er_recv > 0) {
                                    fprintf(stderr, "%s\n", receiveMessage);
                                    if (strcmp(receiveMessage, "NF") != 0) {
                                        sprintf(dirfname, "%s/%s", dir_name, fname);
                                        if ((fp = fopen(dirfname, "wb")) != NULL) { //Open file for writing
                                            char ok_msg[4] = {};
                                            sprintf(ok_msg, "OK");
                                            int len = strlen(ok_msg);
                                            int sent = sendall(localSocket, ok_msg, &len);  //Send ready message
                                            sscanf(receiveMessage, "%lld", &f_len);
                                            int rec_file = my_receive(localSocket, fp, f_len);
                                            if (rec_file >= 0)
                                                fprintf(stderr, "File received successfully\n");
                                            if (fclose(fp) == EOF)
                                                fprintf(stderr, "Error closing file\n");
                                        }
                                        else {
                                            char er_ok[4] = {};
                                            sprintf(er_ok, "ER");
                                            int len = strlen(er_ok);
                                            int sent = sendall(localSocket, er_ok, &len);
                                            fprintf(stderr, "Error opening file for writing\n");
                                            if (sent < 0)
                                                fprintf(stderr, "Error sending error msg\n");
                                        }
                                    }
                                    else 
                                        fprintf(stderr, "The %s doesn't exist\n", fname);
                                }
                                else {
                                    if (er_recv == 0)
                                        fprintf(stderr, "Connection closed\n");
                                    else
                                        perror("Recv error ");
                                }
                            }
                            else {
                                if (strcmp(input_cl, "play") == 0) {
                                    parse_inst(localSocket, '3', fname);
                                    bzero(receiveMessage,sizeof(char)*BUFF_SIZE);
                                    int er_recv = recv(localSocket, receiveMessage, sizeof(char)*BUFF_SIZE, 0);
                                    if (recv > 0) { //Receive data of the video to stream
                                        if (strcmp(receiveMessage, "NF") != 0) {
                                            if (strcmp(receiveMessage, "WF") != 0) {
                                                long long int vidlen;
                                                sscanf(receiveMessage, "%lld", &vidlen);
                                                int play_er = my_rep(localSocket);
                                                fprintf(stderr, "Success\n");
                                            }
                                            else {
                                                fprintf(stderr, "The %s is not a mpg file.\n", fname);
                                            }
                                        }
                                        else {
                                            fprintf(stderr, "The %s doesn't exist.\n", fname);
                                        }
                                    }
                                    else {
                                        if (er_recv == 0)
                                            fprintf(stderr, "Connection closed\n");
                                        else
                                            perror("Recv error ");
                                    }
                                }
                                else {
                                    if (strcmp(input_cl, "0") != 0) 
                                        fprintf(stderr, "Command not found.\n");
                                    else
                                        raise(SIGINT);
                                }
                            }
                        }
                    }
                    break;
                }
                else {
                    bzero(receiveMessage,sizeof(char)*BUFF_SIZE);
                    if ((n_rec = recv(localSocket, receiveMessage, sizeof(char)*BUFF_SIZE, 0)) <= 0) {
                        if (n_rec == 0) {
                            fprintf(stderr, "Connection closed\n");
                            break;
                        }
                        perror("Recv error ");
                    }
                    else {
                        if (strcmp(receiveMessage, "XD") == 0)
                            raise(SIGINT);
                    }
                }
            }
        }
    }
    fprintf(stderr, "Close Socket\n");
    close(localSocket);
    return 0;
}

