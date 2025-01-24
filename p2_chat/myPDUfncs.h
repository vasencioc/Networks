// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//
// PDU creation and send and receive functions

#ifndef __MYPDUFNCS_H__
#define __MYPDUFNCS_H__

#include <stdint.h>

int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData);

int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize); 

#endif