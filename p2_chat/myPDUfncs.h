// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//
// PDU creation and send and receive functions

#ifndef __MYPDUFNCS_H__
#define __MYPDUFNCS_H__

#include <stdint.h>

int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData);

int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize); 

char *unpackHandle(uint8_t *handleBuff, uint8_t numChars);

uint8_t *packHandle(char *handleStr);

int sendFlag(int socket, uint8_t flag);

#endif