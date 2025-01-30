# # Makefile for CPE464 tcp test code
# # written by Hugh Smith - April 2019

# CC= gcc
# CFLAGS= -g -Wall -std=gnu99
# LIBS = 

# OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o myPDUfncs.o

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

# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019


tableTest: tableTest.o HandleTable.o
	gcc -g -o tableTest tableTest.o HandleTable.o

debug: tableTest.o HandleTable.o
	gcc -Wall -Wextra -Werror -ansi -pedantic -g -o tableTest tableTest.c HandleTable.o

test: debug
	valgrind --leak-check=full --show-leak-kinds=all ./tableTest

HandleTable.o: HandleTable.c HandleTable.h
	gcc -Wall -Wextra -Werror -ansi -pedantic -g -c HandleTable.c

# clean up
clean:
	rm -f *.o *~
