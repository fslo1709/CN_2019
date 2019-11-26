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

#define BUFF_SIZE 1024
#define FLEN 300

int senduall(int s, uchar *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { 
        	perror("Send error ");
        	break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int rec_uall(int s, uchar *buf, int *len) {
    int total = 0;  //how many bytes we've read
    int bytesleft = *len;    //how many we have left to receive
    int n;
    while (total < *len) {
        if ((n = recv(s, buf+total, sizeof(char)*bytesleft,0)) <= 0) {
            if (n == 0)
                fprintf(stderr, "Connection closed\n");
            if (n < 0)
                perror("Recv error ");
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total;   // return number actually sent here

    return n<=0?-1:0; // return -1 on failure, 0 on success
}

int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { 
        	perror("Send error ");
        	break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int rec_all(int s, char *buf, int *len) {
    int total = 0;  //how many bytes we've read
    int bytesleft = *len;    //how many we have left to receive
    int n;
    while (total < *len) {
        if ((n = recv(s, buf+total, sizeof(char)*bytesleft,0)) <= 0) {
            if (n == 0)
                fprintf(stderr, "Connection closed\n");
            if (n < 0)
                perror("Recv error ");
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total;   // return number actually sent here

    return n<=0?-1:0; // return -1 on failure, 0 on success
}

int rec_tilln(int s, char *buf) {
	int total = 0;
	int n;
    while (1) {
        if ((n = recv(s, buf+total, sizeof(char)*FLEN,0)) <= 0) {
            if (n == 0)
                fprintf(stderr, "Connection closed\n");
            if (n < 0)
                perror("Recv error ");
            break;
        }
        total += n;
        if (strchr(buf, '\n') != NULL)
        	break;
    }

    return n<=0?-1:0; // return -1 on failure, 0 on success
}

int my_transfer(int s, FILE *fp, long long int len) {
    long long int bytessent = 0;
    char tempbuf[BUFF_SIZE] = {};
    char rec_mess[BUFF_SIZE] = {};
    char size_pack[5] = {};
    while (bytessent < len) {
        bzero(tempbuf, sizeof(char)*BUFF_SIZE);
        bzero(rec_mess, sizeof(char)*BUFF_SIZE);
        long long int l;
        if (len - bytessent > BUFF_SIZE)    //Size of next package
            l = BUFF_SIZE;
        else
            l = len - bytessent;
        sprintf(size_pack, "%lld", l);
        int templ = strlen(size_pack);
        int n = sendall(s, size_pack, &templ);  //Send info
        if (n < 0) {
            fprintf(stderr, "Error sending size of packet after %lld bytes sent\n", bytessent);
            return -1;
        }
        templ = 2;
        n = rec_all(s, rec_mess, &templ);    //Wait for OK
        if (strcmp(rec_mess, "OK") != 0) {
            fprintf(stderr, "Error in recieving side after %lld bytes sent\n", bytessent);
            return -1;
        }  
        fseek(fp, bytessent, SEEK_SET);         //After OK, send the package
        fread(tempbuf, sizeof(char), l, fp);
        n = sendall(s, tempbuf, &l);
        if (n < 0) {
            fprintf(stderr, "Transfer error after %lld bytes sent\n", bytessent);
            return -1;
        }
        bzero(rec_mess, sizeof(char)*BUFF_SIZE);
        n = rec_all(s, rec_mess, &templ);    //Wait for OK
        if (strcmp(rec_mess, "OK") != 0) {
            fprintf(stderr, "Error in receiving side after %lld bytes sent\n", bytessent);
            return -1;
        }
        bytessent += l;
    }
    return 0;
}

int my_receive(int s, FILE *fp, long long int size) {
    long long int bytesreceived = 0;
    char rec_message[BUFF_SIZE] = {};
    char ok_mes[5] = {};
    sprintf(ok_mes, "OK");
    char nk_mes[5] = {};
    sprintf(nk_mes, "NK");
    while (bytesreceived < size) {
        bzero(rec_message, sizeof(char)*BUFF_SIZE);
        int n = recv(s, rec_message, sizeof(char)*BUFF_SIZE, 0);    //get size of package
        if (n <= 0) {
            fprintf(stderr, "Didn't receive package size after %lld bytes\n", bytesreceived);
            return -1;
        }
        long long int len;
        sscanf(rec_message, "%lld", &len);
        if (len > 0 && len <= BUFF_SIZE) {
            int templen = strlen(ok_mes);
            n = sendall(s, ok_mes, &templen);               //Send OK message
            if (n<0) {
                fprintf(stderr, "Error sending OK message after %lld bytes\n", bytesreceived);
                return -1;
            }
            bzero(rec_message, sizeof(char)*BUFF_SIZE);
            n = rec_all(s, rec_message, &len);          //rec package
            if (n < 0) {
                fprintf(stderr, "Error receiving package after %lld bytes\n", bytesreceived);
                return -1;
            }
            fwrite(rec_message, sizeof(char), len, fp);
            templen = strlen(ok_mes);
            n = sendall(s, ok_mes, &templen);
            if (n< 0) {
                fprintf(stderr, "Error sending second OK message after %lld bytes\n", bytesreceived);
                return -1;
            }
            bytesreceived += len;
        }
        else {
            int templen = strlen(nk_mes);
            n = sendall(s, nk_mes, &templen);
            if (n < 0)
                fprintf(stderr, "Error sending failure message\n");
            else 
                fprintf(stderr, "Error in package size received\n");
            return -1;
        }
    }
    return 0;
}