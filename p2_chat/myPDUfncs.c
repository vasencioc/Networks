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
char *unpackHandle(uint8_t *handleBuff, uint8_t numChars){
    static char packed[numChars + 1]; //add space for null
    memcpy(packed, handleBuff + 1, numChars);
    packed[numChars] = '\0';
    return packed;
}

/*Converts character array into byte array and preceded by handle length (without null terminator) and flag*/
uint8_t *packHandle(char *handleStr, uint8_t numChars){
    //uint8_t length = strlen(handleStr); //handle length excluding null
    static uint8_t unpacked[numChars + 1]; //handle - null + length field
    memcpy(unpacked, &length, 1);  //handle preceded by its length
    memcpy(unpacked + 1, handleStr, length);  
    return unpacked;
}

int sendFlag(int socket, uint8_t flag){
	uint8_t response[1];
	response[0] = flag;
	int sent = sendPDU(socket, response, 1);
	return sent;
}
