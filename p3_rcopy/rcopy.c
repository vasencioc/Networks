// 
// Writen by Victoria Asencio-Clemens, March 2025
//
// Client setup and control for rcopy
#include "bufferLib.h"
#include "PDU.h"

/*** States for FSM ***/
typedef enum {
	SETUP_REQ,
	SETUP_REPLY,
	IN_ORDER,
	BUFFERING,
	FLUSHING,
	TEARDOWN,
	END
} STATE;

int checkConfig(int socketNum, char * fromFile, char * toFile, uint32_t windowSize, uint32_t bufferSize);
void clientControl(char *fromFile, int toFD, uint32_t windowSize, uint16_t buffSize, int socketNum, struct sockaddr_in6 * server);
STATE transferRequest(int socketNum, uint32_t windowSize, uint32_t bufferSize, char *from_filename, struct sockaddr_in6 *serverAddress, uint32_t *seqNum, uint32_t *reqCount);
STATE transferReply(int socketNum, struct sockaddr_in6 *server, uint32_t *requestCount, uint32_t *seqNum, char *fromFile, int *addrLen);
//STATE receiveSetup(int socketNum, uint32_t windowSize, uint32_t bufferSize, char *from_filename, struct sockaddr *serverAddress, int addressLen);
STATE receiveData(int socketNum, struct sockaddr_in6* serverAddress, int toFD, uint16_t buffSize, uint32_t *expectedSeqNum, uint32_t *highestSeqNum, uint32_t *sequenceNum, int *addrLen);
STATE sendEOFack(int socketNum, uint32_t *sequenceNum, int toFD, struct sockaddr_in6 *serverAddress, int *addrLen);
STATE flushData(int socketNum, struct sockaddr_in6* serverAddress, int toFD, uint16_t buffSize, uint32_t *expectedSeqNum, uint32_t *highestSeqNum, uint32_t *sequenceNum);
void sendRRSREJ(int socketNum, struct sockaddr_in6* serverAddress, uint32_t *sequenceNum, uint32_t *expected, int flag);
STATE bufferData(int socketNum, struct sockaddr_in6* serverAddress, int toFD, uint16_t buffSize, uint32_t *expectedSeqNum, uint32_t *highestSeqNum, uint32_t *sequenceNum, int *addrLen);
int timer();
int checkArgs(int argc, char * argv[]);

/*** Source Code ***/
CircularBuff* packetBuff;

int main (int argc, char *argv[])
 {
	int socketNum = 0;				
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	sendtoErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
	socketNum = setupUdpClientToServer(&server, argv[6], portNumber);
	int toFD = checkConfig(socketNum, argv[1], argv[2], atoi(argv[3]), atoi(argv[4]));
	if(toFD < 0){ // unable to open file
		printf("Error on open of output file: %s\n", argv[2]);
	} else {
		clientControl(argv[1], toFD, atoi(argv[3]), atoi(argv[4]), socketNum, &server);
	}
	close(socketNum);

	return 0;
}

int checkConfig(int socketNum, char * fromFile, char * toFile, uint32_t windowSize, uint32_t bufferSize){
	if(strlen(fromFile) > MAX_FILENAME){
		printf("Invalid handle, handle longer than 100 characters: %s\n", fromFile);
		close(socketNum);
		exit(-1);
	} else if((windowSize > MAX_WINDOW) || (windowSize <= 0)){
		printf("Invalid window size\n");
		close(socketNum);
		exit(-1);
	}else if((bufferSize > MAX_PDU) || (bufferSize <= 0)){
		printf("Invalid buffer size\n");
		close(socketNum);
		exit(-1);
	}
	return open(toFile, (O_WRONLY | O_CREAT | O_TRUNC), 0644);
}

/*Controls client communication to/from server*/
void clientControl(char *fromFile, int toFD, uint32_t windowSize, uint16_t buffSize, int socketNum, struct sockaddr_in6 * server){
	// setup variables for sending data
	packetBuff = createBuffer(windowSize, buffSize);
	uint32_t sequenceNum = 0;
	uint32_t expected = 1;
	uint32_t highest = 0;
	uint32_t requestCount = 0;
	int serverLen = sizeof(struct sockaddr_in6);
	// setup poll set for receiving data
	setupPollSet();
	addToPollSet(socketNum);
	// FSM for flow control
	STATE presentState = SETUP_REQ;
	while(presentState != END){
		switch(presentState) {
			case(SETUP_REQ): presentState = transferRequest(socketNum, windowSize, buffSize, fromFile, server, &sequenceNum, &requestCount); break;
			case(SETUP_REPLY): presentState = transferReply(socketNum, server, &requestCount, &sequenceNum, fromFile, &serverLen); break;
			case(IN_ORDER): presentState = receiveData(socketNum, server, toFD, buffSize, &expected, &highest, &sequenceNum, &serverLen); break;
			case(BUFFERING): presentState = bufferData(socketNum, server, toFD, buffSize, &expected, &highest, &sequenceNum, &serverLen); break;
			case(FLUSHING): presentState = flushData(socketNum, server, toFD, buffSize, &expected, &highest, &sequenceNum); break;
			case(TEARDOWN): presentState = sendEOFack(socketNum, &sequenceNum, toFD, server, & serverLen); break;
			case(END): break;
			default: break;
		}
	}
	//clean up
	destroyBuffer(packetBuff);
	close(toFD);
	removeFromPollSet(socketNum);
}

STATE transferRequest(int socketNum, uint32_t windowSize, uint32_t bufferSize, char *from_filename, struct sockaddr_in6 *serverAddress, uint32_t *seqNum, uint32_t *reqCount){
	int addressLen = sizeof(struct sockaddr_in6);
	uint32_t dataLen = strlen(from_filename) + 7; //add space for null term, buffer and window length
	uint8_t data[dataLen];
	memcpy(data, &windowSize, 4);
	memcpy(data + 4, &bufferSize, 2);
	memcpy(data + 6, from_filename, strlen(from_filename) + 1);
	//data[dataLen] = '\0';
	uint8_t *filePDU = buildPDU(data, dataLen, 0, FLAG_FILE_REQ); //build filename PDU
	safeSendto(socketNum, filePDU, dataLen + HEADER_LEN, 0, (struct sockaddr *)serverAddress, addressLen);
	(*reqCount)++;
	return SETUP_REPLY;
}

STATE transferReply(int socketNum, struct sockaddr_in6 *server, uint32_t *requestCount, uint32_t *seqNum, char *from_filename, int *addrLen){
	if(pollCall(1000)){
		uint8_t pduBuff[MAX_PDU];
		int pduLen = safeRecvfrom(socketNum, pduBuff, MAX_PDU, 0, (struct sockaddr *)server, addrLen);
		if (in_cksum((uint16_t *) pduBuff, pduLen) != 0) {
			(*seqNum)++;
			(*requestCount)--;
			if (*requestCount > 9) { //no more tries
				printf("Server closed\n");
				return END;
			}
			else {
				return SETUP_REQ;
			}
		}
		struct pdu *received = (struct pdu *)pduBuff;
		if(received->flag == FLAG_FILE_RES){
			if(received->payload[0]){//received file Ack
				return IN_ORDER;
			} else { //received file Nack
				printf("Error: file %s not found\n", from_filename);
				return END;
			}
		}
	}
	if (*requestCount == 10){printf("Server Closed\n"); return END;}
	return SETUP_REQ;
}

STATE receiveData(int socketNum, struct sockaddr_in6* serverAddress, int toFD, uint16_t buffSize, uint32_t *expectedSeqNum, uint32_t *highestSeqNum, uint32_t *sequenceNum, int *addrLen){
	printf("getting here\n");
	int timerExpired = timer();
	//printf("getting here 2\n");
	if(timerExpired) return END;
	uint8_t PDU[buffSize + HEADER_LEN];
	int pduLen = safeRecvfrom(socketNum, PDU, buffSize + HEADER_LEN, 0, (struct sockaddr *)serverAddress, addrLen);
	struct pdu *received = (struct pdu *)PDU; //format into struct
	uint32_t seqNumRecv = ntohl(received->sequenceNum);
	if(received->flag == FLAG_EOF){
		if (*expectedSeqNum == *highestSeqNum) {
			return TEARDOWN;
		} else {
			sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_SREJ);
			return BUFFERING;
		}
	} else if(in_cksum((uint16_t *) PDU, pduLen) == 0){
		if(seqNumRecv > *expectedSeqNum){
			sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_SREJ);
			*highestSeqNum = seqNumRecv;
			addBuffVal(packetBuff, PDU, buffSize + HEADER_LEN, seqNumRecv);
			return BUFFERING;
		} else if(seqNumRecv == *expectedSeqNum){
			write(toFD, received->payload, pduLen - 7);
			sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_RR);
			*highestSeqNum = *expectedSeqNum;
			(*expectedSeqNum)++;
		} else{ // received lower than expected
			sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_RR);
		}
	}
	return IN_ORDER;
}

STATE sendEOFack(int socketNum, uint32_t *sequenceNum, int toFD, struct sockaddr_in6 *serverAddress, int *addrLen){
	uint8_t placeholder = 0;
	uint8_t *EOFack = buildPDU((uint8_t *)&placeholder, 1, *sequenceNum, FLAG_EOF_ACK);
	safeSendto(socketNum, EOFack, HEADER_LEN + 1, 0, (struct sockaddr *)serverAddress, *addrLen);
	(*sequenceNum)++;
	if(timer()) printf("Server Closed after EOF\n"); return END;
	safeRecvfrom(socketNum, EOFack, HEADER_LEN + 1, 0, (struct sockaddr *)serverAddress, addrLen);
	if (EOFack[4] == FLAG_EOF_ACK) return END;
	return TEARDOWN;
}

void sendRRSREJ(int socketNum, struct sockaddr_in6* serverAddress, uint32_t *sequenceNum, uint32_t *expected, int flag){
	uint32_t expected_Net = htonl(*expected);
	uint8_t RRSREJdata[4];
	memcpy(RRSREJdata, &expected_Net, 4);
	uint8_t *RRSREJ = buildPDU(RRSREJdata, 4, *sequenceNum, flag); //build filename PDU
	safeSendto(socketNum, RRSREJ, RRSREJ_LEN, 0, (struct sockaddr *)serverAddress, (int) sizeof(struct sockaddr_in6));
	(*sequenceNum)++;
}

STATE bufferData(int socketNum, struct sockaddr_in6* serverAddress, int toFD, uint16_t buffSize, uint32_t *expectedSeqNum, uint32_t *highestSeqNum, uint32_t *sequenceNum, int *addrLen){
	int timerExpired = timer();
	if(timerExpired) return END;
	uint8_t PDU[buffSize + HEADER_LEN];
	int pduLen = safeRecvfrom(socketNum, PDU, buffSize + HEADER_LEN, 0, (struct sockaddr *)serverAddress, addrLen);
	struct pdu *received = (struct pdu *)PDU; //format into struct
	uint32_t seqNumRecv = ntohl(received->sequenceNum);
	if(in_cksum((uint16_t *) PDU, pduLen) == 0){
		if(seqNumRecv > *expectedSeqNum){
			*highestSeqNum = seqNumRecv;
			addBuffVal(packetBuff, PDU, buffSize + HEADER_LEN, seqNumRecv);
		} else if(seqNumRecv == *expectedSeqNum){
			write(toFD, received->payload, pduLen - 7);
			(*expectedSeqNum)++;
			return FLUSHING;
		} else{
			sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_RR);
		}
	}
	return BUFFERING;
}

STATE flushData(int socketNum, struct sockaddr_in6* serverAddress, int toFD, uint16_t buffSize, uint32_t *expectedSeqNum, uint32_t *highestSeqNum, uint32_t *sequenceNum){
	while((*expectedSeqNum <= *highestSeqNum) && validityCheck(packetBuff, *expectedSeqNum)){
		BufferVal flushVal = getBuffVal(packetBuff, *expectedSeqNum);
		write(toFD, flushVal.PDU + 7, flushVal.dataLen);
		(*expectedSeqNum)++;
		setInvalid(packetBuff, *expectedSeqNum);
	}
	if (*expectedSeqNum == *highestSeqNum) { //all caught up, back to in order
		sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_RR);
		return IN_ORDER;
	} else {
		sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_RR);
		sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_SREJ);
	}
	return BUFFERING;
}

int timer(){
	int testSocket = 0;
	if (!((testSocket = pollCall(10000)) > 0)) { //timer expired
		printf("Server Closed\n");
		return 1;
	}
	return 0;
}

int checkArgs(int argc, char * argv[])
{

        int portNumber = 0;
	
        /* check command line arguments  */
	if (argc != 8)
	{
		printf("usage: %s from-filename to-filename window-size buffer-size error-rate remote-machine remote-port\n", argv[0]);
		exit(1);
	}
	
	portNumber = atoi(argv[7]);
		
	return portNumber;
}
