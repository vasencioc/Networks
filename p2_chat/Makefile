# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019

CC= gcc
CFLAGS= -g -Wall -std=gnu99
LIBS = 

OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o myPDUfncs.o

all:   cclient2 server2

cclient2: cclient2.c $(OBJS)
	$(CC) $(CFLAGS) -o cclient2 cclient2.c  $(OBJS) $(LIBS)

server2: server2.c $(OBJS)
	$(CC) $(CFLAGS) -o server2 server2.c $(OBJS) $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f server2 cclient2 *.o




