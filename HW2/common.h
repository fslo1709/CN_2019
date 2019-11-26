//TODO: add header guard

int senduall(int s, uchar *buf, int *len);
int rec_uall(int s, uchar *buf, int *len);
int sendall(int s, char *buf, int *len);
int rec_all(int s, char *buf, int *len);
int rec_tilln(int s, char *buf);
int my_transfer(int s, FILE *fp, long long int len);
int my_receive(int s, FILE *fp, long long int size);