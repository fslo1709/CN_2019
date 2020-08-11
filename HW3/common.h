//TODO: add header guard
#define MAXBUFF 1000

typedef struct header_tag {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
} header;

typedef struct segment_tag{
	header head;
	char data[MAXBUFF];
} segment;

void init_header(header *src, int ack);
void setIP(char *dst, char *src);
