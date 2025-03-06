// 
// Writen by Victoria Asencio-Clemens, March 2025
//
// Client setup and control for rcopy

#include "poll.h"
#include "networks.h"
#include "safeUtil.h"
#include "bufferLib.h"
#include "PDU.h"

/*** States for FSM ***/
typedef enum {
	SETUP,
	IN_ORDER,
	BUFFERING,
	FLUSHING,
	TEARDOWN,
	END
} STATE;

int checkConfig(char * fromFile, int toFD, uint32_t windowSize, uint32_t bufferSize);
STATE receiveSetup(int socketNum, uint32_t windowSize, uint32_t bufferSize, char *from_filename, struct sockaddr *serverAddress, int addressLen);
void clientControl(char *fromFile, int toFD, uint32_t windowSize, uint32_t buffSize, int socketNum, struct sockaddr_in6 serverAddress);
STATE receiveSetup(int socketNum, uint32_t windowSize, uint32_t bufferSize, char *from_filename, struct sockaddr *serverAddress, int addressLen);
STATE receiveData(int socketNum, struct sockaddr* serverAddress, int buffSize, int expectedSeqNum, int highestSeqNum, int sequenceNum);
STATE sendEOFack(uint32_t sequenceNum, int toFD, struct sockaddr *serverAddress, int addressLen);
void sendRRSREJ(int socketNum, struct sockaddr* serverAddress, int sequenceNum, int expected, int flag);
STATE bufferData(socketNum, serverAddress, toFD, buffSize, expected);

/*** Source Code ***/
int main(int argc, char * argv[]){
	int socketNum = 0;         //socket descriptor
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
    struct sockaddr_in6 serverAddress;
    char hostName;
    int serverPort;
	int socketNum = setupUdpClientToServer(serverAddress, hostName, argv[7]);
    int toFD = checkConfig(argv[1], arv[2], argv[3], argv[4]);
	if(toFD < 0){ // unable to open file
		print("Error on open of output file: %s.", argv[2]);
	} else {
		clientControl(argv[1], toFD, argv[3], argv[4], socketNum, serverAddress);
	}
	close(socketNum);
	return 0;
}

/* Logs client into server if the connection is valid and the handle is unique */
int checkConfig(char * fromFile, char * toFile, uint32_t windowSize, uint32_t bufferSize){
	if(strlen(fromFile) > MAX_FILENAME){
		printf("Invalid handle, handle longer than 100 characters: %s\n", fromFile);
		close(socketNum);
		exit(-1);
	} else if((windowSize > MAX_WINDOW) || (windowSize <= 0)){
		printf("Invalid window size\n");
		close(socketNum);
		exit(-1);
	}else if((bufferSize > MAX_BUFFER) || (bufferSize <= 0)){
		printf("Invalid buffer size\n");
		close(socketNum);
		exit(-1);
	}
	return open(toFile, O_WRONLY);
}

/*Controls client communication to/from server*/
void clientControl(char *fromFile, int toFD, uint32_t windowSize, uint32_t buffSize, int socketNum, struct sockaddr_in6 serverAddress){
	// setup variables for sending data
	struct pdu *received = (struct pdu *)pdu; //initial pdu
	int sequenceNum = 0;
	int expected = 0;
	int highest;
	// setup poll set for receiving data
	setupPollSet();
	addToPollSet(socketNum);
	// FSM for flow control
	STATE presentState = SETUP;
	while(presentState != END){
		switch(presentState) {
			case(SETUP):{
				presentState = receiveSetup(socketNum, windowSize, buffSize, fromFile, serverAddress, sizeof(serverAddress));
				break;
			}
			case(IN_ORDER): presentState = receiveData(socketNum, serverAddress, toFD, buffSize, expected, sequenceNum);
			case(BUFFERING): presentState = bufferData(socketNum, serverAddress, toFD, buffSize, expected);
			case(FLUSHING): presentState = flushData(socketNum, serverAddress, toFD, buffSize, expected, highest, sequenceNum);
			case(TEARDOWN): presentState = sendEOFack(sequenceNum, toFD, serverAddress, sizeof(serverAddress));
			case(END): break;
			default: break;
		}
	}
	//clean up
	removeFromPollSet(socketNum);
	clientClosed(socketNum);
}

STATE receiveSetup(int socketNum, uint32_t windowSize, uint32_t bufferSize, char *from_filename, struct sockaddr *serverAddress, int addressLen){
	uint32_t payloadLen = strlen(fromFile) + 8; //add space for buffer and window length
	uint8_t filePDU[payloadLen + HEADER_LEN];
	filePDU = buildPDU(fromFile, payloadLen, 0, FLAG_FILE_REQ); //build filename PDU
	count = 0; //initialize count
	safeSendTo(socketNum, filePDU, payloadLen + HEADER_LEN, 0, serverAddress, addressLen);
	if(pollCall(1000)){
		uint8_t *pduBuff[HEADER_LEN + 1];
		int pduLen = safeRecvFrom(socketNum, pduBuff, HEADER_LEN + 1, 0, serverAddress, addressLen);
		struct pdu *received = (struct pdu *)pduBuff;
		if(received.flag == FLAG_FILE_RES){
			if(received.payload){//received file Ack
				return IN_ORDER;
			} else { //received file Nack
				printf("Error: file %s not found", from_filename);
				return END;
			}
		} else{
			safeSendTo(socketNum, filePDU, payloadLen + HEADER_LEN, 0, serverAddress, addressLen); //resend file request packet
			count++; //increment count of resends
			if(count == 10) printf("Server Closed\n"); return END;
		}
	} else{
		safeSendTo(socketNum, filePDU, payloadLen + HEADER_LEN, 0, serverAddress, addressLen); //resend file request packet
		count++; //increment count of resends
		if(count == 10) printf("Server Closed\n"); return END;
	}
	return DONE;
}

STATE receiveData(int socketNum, struct sockaddr* serverAddress, int toFD, int buffSize, int expectedSeqNum, int highestSeqNum, int sequenceNum){
	uint8_t *PDU[buffSize + HEADER_LEN];
	int pduLen = safeRecvFrom(socketNum, PDU, buffSize + HEADER_LEN, 0, serverAddress, sizeof(serverAddress));
	struct pdu *received = (struct pdu *)PDU; //format into struct
	if(received.sequenceNum > expectedSeqNum){
		sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_SREJ);
		highestSeqNum = received.sequenceNum;
		addVal(packetBuff, PDU, buffSize + HEADER_LEN, received.sequenceNum);
		return BUFFERING;
	} else if(received.sequenceNum == expectedSeqNum){
		write(toFD, received.payload, pduLen - 7);
		highestSeqNum = expectedSeqNum;
		expectedSeqNum++;
		sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_RR);
	}
	return IN_ORDER;
}

STATE sendEOFack(uint32_t sequenceNum, int toFD, struct sockaddr *serverAddress, int addressLen){
	uint8_t *EOFack = buildPDU(0, 0, sequenceNum, FLAG_EOF_ACK); //build filename PDU
	safeSendTo(socketNum, filePDU, HEADER_LEN, 0, serverAddress, addressLen);
	close(toFD);
	return END;
}

void sendRRSREJ(int socketNum, struct sockaddr* serverAddress, int sequenceNum, int expected, int flag){
	uint8_t *RRSREJ = buildPDU(expected, 4, sequenceNum, flag); //build filename PDU
	safeSendTo(socketNum, RRSREJ, RRSREJ_LEN, 0, serverAddress, sizeof(serverAddress)); 
}

STATE bufferData(socketNum, serverAddress, toFD, buffSize, expected){
	uint8_t *PDU[buffSize + HEADER_LEN];
	int pduLen = safeRecvFrom(socketNum, PDU, buffSize + HEADER_LEN, 0, serverAddress, sizeof(serverAddress));
	struct pdu *received = (struct pdu *)PDU; //format into struct
	if(received.sequenceNum > expectedSeqNum){
		highestSeqNum = received.sequenceNum;
		addVal(packetBuff, PDU, buffSize + HEADER_LEN, received.sequenceNum);
	} else if(received.sequenceNum == expectedSeqNum){
		write(toFD, received.payload, pduLen - 7);
		expectedSeqNum++;
		return FLUSHING;
	}
	return BUFFERING;
}

STATE flushData(socketNum, serverAddress, toFD, buffSize, expected, highest, sequenceNum){
	uint32_t i = 0;
	while((packetBuff.buffer[i]->validFlag) && (packetBuff.buffer[i]->sequenceNum == expected)){
		write(toFD, packetBuff.buffer[i]->PDU + 7, dataLen);
		expected++;
		i++;
	}
	if((expected < highest) && (!packetBuff.buffer[i]->validFlag)){
		//WHAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
		return BUFFERING;
	} else if(expected == highest){
		write(toFD, packetBuff.buffer[i]->PDU + 7, dataLen);
		expected++;
		sendRRSREJ(socketNum, serverAddress, sequenceNum, expected, FLAG_RR);
	}
	
}