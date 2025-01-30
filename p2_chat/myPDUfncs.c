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
 #include "networks.h"
 #include "safeUtil.h"

/**
 *  Create the PDU and send the PDU 
 * @param clientSocket client socket number
 * @param dataBuffer buffer of data to be sent
 * @param lengthOfData the size of data to be sent
 * @return data bytes sent
 */
int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData){
    int bytesSent = 0;
    //calculate PDU length and make big enough PDU buffer
    uint16_t lengthOfPDU = lengthOfData + 2;
    uint8_t PDU[lengthOfPDU];
    //build PDU
    uint16_t lenPDUNetOrder = htons(lengthOfPDU);
    memcpy(PDU, &lenPDUNetOrder, 2);
    memcpy(PDU + 2, dataBuffer, lengthOfData);
    //send PDU
    bytesSent = send(clientSocket, PDU, lengthOfPDU, 0);
    return bytesSent; //return num bytes sent (length of PDU)
}

/**
 * recv() the PDU and pass back the dataBuffer
 * @param socketNumber server socket number
 * @param dataBuffer buffer for PDU
 * @param bufferSize size of buffer for PDU
 * @return data bytes received
 */
int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize){
    int16_t PDUlenNetOrder, PDUlenHostOrder;
    //get message length
    int bytesReceived = safeRecv(socketNumber, dataBuffer, 2, MSG_WAITALL);
    if (bytesReceived == 0) //check for closed connection
    {
        return bytesReceived;
    }
    memcpy(&PDUlenNetOrder, dataBuffer, 2);
    PDUlenHostOrder = ntohs(PDUlenNetOrder);
    //get message
    if (bufferSize < PDUlenHostOrder) {  //Exit if buffer is not large enough to receive PDU
        printf("Buffer Error\n");
        exit(-1);
    }
    bytesReceived = recv(socketNumber, dataBuffer, PDUlenHostOrder - 2, MSG_WAITALL);
    //return message length (== 0 if connection closed)
    return bytesReceived ;
}

/*Converts byte array into character array with null terminator*/
char *packMessage(uint8_t *message uint16_t messageLen){
    char *packed[messageLen + 1];
    packed = (char *)message;
    packed[messageLen] = '\n';
    return packed;
}

/*Converts character array into byte array without null terminator*/
uint8_t *unpackMessage(char *message){
    uint8_t *unpacked = (uint8_t *)malloc(strlen(message));
    memcpy(unpacked, message, strlen(message)); 
    return unpacked;
}
