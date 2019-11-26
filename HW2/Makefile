CC = g++
OPENCV =  `pkg-config --cflags --libs opencv`
PTHREAD = -pthread

CLIENT = client.cpp
SERVER = server.cpp
CLI = client
SER = server
COM = common.cpp

all: server client
  
server: $(SERVER)
	$(CC) $(SERVER) $(COM) -o $(SER)  $(OPENCV) $(PTHREAD) 
client: $(CLIENT)
	$(CC) $(CLIENT) $(COM) -o $(CLI)  $(OPENCV) $(PTHREAD)

.PHONY: clean

clean:
	rm $(CLI) $(SER)
