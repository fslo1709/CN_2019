CC = g++
OPENCV =  `pkg-config --cflags --libs opencv`

SENDER = sender.cpp
RECEIVER = receiver.cpp
AGENT = agent.cpp
SEN = sender
REC = receiver
AGE = agent
COM = common.cpp

all: sender receiver agent
  
sender: $(SENDER)
	$(CC) $(SENDER) $(COM) -o $(SEN)  $(OPENCV)
receiver: $(RECEIVER)
	$(CC) $(RECEIVER) $(COM) -o $(REC)  $(OPENCV)
agent: $(AGENT)
	$(CC) $(AGENT) $(COM) -o $(AGE) $(OPENCV)

.PHONY: clean

clean:
	rm $(SEN) $(REC) $(AGE)
