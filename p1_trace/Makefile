# Example makefile for CPE464 Trace program
#
# Just links in pcap library

CC = gcc
LIBS = -lpcap
CFLAGS = -g -Wall -pedantic -std=gnu99
#CGLAGS = 

all:  trace

trace: trace.o checksum.o
	$(CC) $(CFLAGS) -o $@ trace.o checksum.o $(LIBS)

trace.o: trace.c checksum.h
	$(CC) $(CFLAGS) -c trace.c

checksum.o: checksum.c checksum.h
	$(CC) $(CFLAGS) -c checksum.c

clean:
	rm -f trace
