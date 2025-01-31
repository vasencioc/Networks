# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019
# modified by Victoria Asencio-Clemens - January 2025

CC= gcc
CFLAGS= -g -Wall -std=gnu99
LIBS = 

OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o myPDUfncs.o HandleTable.o

all:   cclient server

cclient: cclient.c $(OBJS)
	$(CC) $(CFLAGS) -o cclient cclient.c  $(OBJS) $(LIBS)

server: server.c $(OBJS)
	$(CC) $(CFLAGS) -o server server.c $(OBJS) $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f server cclient *.o

# CC = gcc
# CFLAGS = -Wall -Wextra -Werror -std=gnu99 -pedantic -g
# LDFLAGS = -g

# tableTest: tableTest.o HandleTable.o
# 	$(CC) $(LDFLAGS) -o tableTest tableTest.o HandleTable.o

# debug: tableTest.o HandleTable.o
# 	$(CC) $(CFLAGS) $(LDFLAGS) -o tableTest tableTest.o HandleTable.o

# test: debug
# 	valgrind --leak-check=full --show-leak-kinds=all ./tableTest

# HandleTable.o: HandleTable.c HandleTable.h
# 	$(CC) $(CFLAGS) -c HandleTable.c

# tableTest.o: tableTest.c HandleTable.h
# 	$(CC) $(CFLAGS) -c tableTest.c

# # Clean up
# clean:
# 	rm -f *.o tableTest *~

