// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//
// PDU creation and send and receive functions
 #include <string.h>
 #include <arpa/inet.h> 
 #include <errno.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include "myPDUfncs.h"

/**
 *  Create the PDU and send the PDU 
 * @param clientSocket client socket number
 * @param dataBuffer buffer of data to be sent
 * @param lengthOfData the size of data to be sent
 * @return data bytes sent
 */
int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData){
    int bytesSent = 0;
    uint16_t lengthOfPDU = lengthOfData + 2;
    uint8_t PDU[lengthOfPDU];
    uint16_t lenPDUNetOrder = htons(lengthOfPDU);
    memcpy(PDU, &lenPDUNetOrder, 2);
    memcpy(PDU, dataBuffer, lengthOfData);
    if((bytesSent = send(clientSocket, PDU, lengthOfPDU, 0)) < 0){
        perror("send call");
        exit(-1);
    }
    return bytesSent;
}

/**
 * recv() the PDU and pass back the dataBuffer
 * @param socketNumber server socket number
 * @param dataBuffer buffer for PDU
 * @param bufferSize size of buffer for PDU
 * @return data bytes received
 */
int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize){
    int16_t PDUlenNetOrder = recv(socketNumber, dataBuffer, 2, MSG_WAITALL);
    if (PDUlenNetOrder <= 0)
    {
        perror("recv call");
        exit(-1);
    }
    int16_t PDUlenHostOrder = ntohs(PDUlenNetOrder);
    if(bufferSize < PDUlenHostOrder ){
        perror("PDU buffer");
        exit(-1);
    }
    int bytesReceived = recv(socketNumber, dataBuffer, PDUlenHostOrder - 2, MSG_WAITALL);
    if (bytesReceived < 0)
    {
        perror("recv call");
        exit(-1);
    }
    return bytesReceived ;
}