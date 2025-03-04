# # Makefile for CPE464 tcp test code
# # written by Hugh Smith - April 2019
# # modified by Victoria Asencio-Clemens - January 2025

# CC= gcc
# CFLAGS= -g -Wall -std=gnu99
# LIBS = 

# OBJS = networks.o gethostbyname.o poll.o safeUtil.o chatHelpers.o HandleTable.o

# all:   cclient server

# cclient: cclient.c $(OBJS)
# 	$(CC) $(CFLAGS) -o cclient cclient.c  $(OBJS) $(LIBS)

# server: server.c $(OBJS)
# 	$(CC) $(CFLAGS) -o server server.c $(OBJS) $(LIBS)

# .c.o:
# 	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

# cleano:
# 	rm -f *.o

# clean:
# 	rm -f server cclient *.o

# Builds targets for sorting arrays in parallel.
# CSC 357, Assignment 7
# Given code, Winter '24

CC = gcc
CFLAGS = -Wall -Wextra -g -std=gnu99
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all 

# Source files
SRCS = windowLib.c test_window_buffer.c
OBJS = $(SRCS:.c=.o)

# Output executable
TARGET = test_window_lib

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

valgrind: $(TARGET)
	$(VALGRIND) ./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
