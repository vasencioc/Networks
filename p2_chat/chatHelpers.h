// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//
// PDU creation and send and receive functions

#ifndef __MYPDUFNCS_H__
#define __MYPDUFNCS_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>
#include <ctype.h>
#include <stdint.h>
#include <arpa/inet.h> 

//#define MAXBUF 1024
#define MAXHANDLE 100
#define MAXPACKET 1400
#define MAXTEXT 199
#define DEBUG_FLAG 1
#define FLAG1 1
#define FLAG2 2
#define FLAG3 3
#define FLAG4 4
#define FLAG5 5
#define FLAG6 6
#define FLAG7 7
#define FLAG10 10
#define FLAG11 11
#define FLAG12 12
#define FLAG13 13
int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData);

int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize); 

char *unpackHandle(uint8_t *handleBuff);

uint8_t *packHandle(char *handleStr);

int sendFlag(int socket, uint8_t flag);

#endif