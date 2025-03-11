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

/*** Function Definitions ***/
int checkConfig(int socketNum, char * fromFile, char * toFile, uint32_t windowSize, uint16_t bufferSize);
void clientControl(char *fromFile, int toFD, uint32_t windowSize, uint16_t buffSize, int socketNum, struct sockaddr_in6 * server);
STATE transferRequest(int socketNum, uint32_t windowSize, uint32_t bufferSize, uint32_t *requestCount, char *from_filename, struct sockaddr_in6 *serverAddress, uint32_t *seqNum, int addressLen);
STATE transferReply(int socketNum, struct sockaddr_in6 *server, uint32_t *requestCount, uint32_t *seqNum, char *fromFile, int *addrLen);
STATE receiveData(int socketNum, struct sockaddr_in6* serverAddress, int toFD, uint16_t buffSize, uint32_t *expectedSeqNum, uint32_t *highestRRSREJ, uint32_t *sequenceNum, int *addrLen);
STATE sendEOFack(int socketNum, uint32_t *sequenceNum, int toFD, struct sockaddr_in6 *serverAddress, int *addrLen);
STATE flushData(int socketNum, struct sockaddr_in6* serverAddress, int toFD, uint16_t buffSize, uint32_t *expectedSeqNum, uint32_t *highestRRSREJ, uint32_t *sequenceNum);
void sendRRSREJ(int socketNum, struct sockaddr_in6* serverAddress, uint32_t *sequenceNum, uint32_t *RRSREJnum, int flag);
STATE bufferData(int socketNum, struct sockaddr_in6* serverAddress, int toFD, uint16_t buffSize, uint32_t *expectedSeqNum, uint32_t *highestRRSREJ, uint32_t *sequenceNum, int *addrLen);
int timer();
int checkArgs(int argc, char * argv[]);

/*** Source Code ***/
CircularBuff* packetBuff; //data buffer

/* Program Entry Point */
int main (int argc, char *argv[])
 {
	// inititalize variables for UDP communication
	int socketNum = 0;
	struct sockaddr_in6 server;
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	sendtoErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON); //for corrupt packet testing
	socketNum = setupUdpClientToServer(&server, argv[6], portNumber); 
	int toFD = checkConfig(socketNum, argv[1], argv[2], atoi(argv[3]), atoi(argv[4])); //check command line argument values
	
	if(toFD < 0){ //can't open file, should never happen
		printf("Error on open of output file: %s\n", argv[2]);
	} else { //download control flow
		clientControl(argv[1], toFD, atoi(argv[3]), atoi(argv[4]), socketNum, &server);
	}
	close(socketNum);

	return 0;
}

/* Check client input configuration */
int checkConfig(int socketNum, char * fromFile, char * toFile, uint32_t windowSize, uint16_t bufferSize){
	if(strlen(fromFile) > MAX_FILENAME){ //check filename length
		printf("Filename is too long: %s\n", fromFile);
		close(socketNum);
		exit(-1);
	} else if((windowSize >= pow(2, 30)) || (windowSize <= 0)){ //check window size
		printf("Invalid window size\n");
		close(socketNum);
		exit(-1);
	}else if((bufferSize > MAX_PAYLOAD) || (bufferSize <= 0)){ //check buffer size
		printf("Invalid buffer size\n");
		close(socketNum);
		exit(-1);
	}
	return open(toFile, (O_WRONLY | O_CREAT | O_TRUNC), 0644); //open or create to file
}

/*Control communication to/from server*/
void clientControl(char *fromFile, int toFD, uint32_t windowSize, uint16_t buffSize, int socketNum, struct sockaddr_in6 * server){
	//setup variables for sending data
	packetBuff = createBuffer(windowSize, buffSize); //create buffer, global variable
	uint32_t sequenceNum = 0; //sending sequence number
	uint32_t expectedSeqNum = 1; //to track expected sequence number
	uint32_t highestRRSREJ= 0; //to track highest RR/SREJ allowed
	uint32_t requestCount = 0; //to track file requests sent
	int serverLen = sizeof(struct sockaddr_in6); //for UDP communication
	// setup poll set for receiving data (timers)
	setupPollSet();
	addToPollSet(socketNum);
	// FSM for flow control
	STATE presentState = SETUP_REQ;
	while(presentState != END){
		switch(presentState) {
			case(SETUP_REQ): presentState = transferRequest(socketNum, windowSize, buffSize, &requestCount, fromFile, server, &sequenceNum, serverLen); break;
			case(SETUP_REPLY): presentState = transferReply(socketNum, server, &requestCount, &sequenceNum, fromFile, &serverLen); break;
			case(IN_ORDER): presentState = receiveData(socketNum, server, toFD, buffSize, &expectedSeqNum, &highestRRSREJ, &sequenceNum, &serverLen); break;
			case(BUFFERING): presentState = bufferData(socketNum, server, toFD, buffSize, &expectedSeqNum, &highestRRSREJ, &sequenceNum, &serverLen); break;
			case(FLUSHING): presentState = flushData(socketNum, server, toFD, buffSize, &expectedSeqNum, &highestRRSREJ, &sequenceNum); break;
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

/* Send file download request */
STATE transferRequest(int socketNum, uint32_t windowSize, uint32_t bufferSize, uint32_t *requestCount, char *from_filename, struct sockaddr_in6 *serverAddress, uint32_t *seqNum, int addressLen){
	//setup variables for sending file request
	uint32_t dataLen = strlen(from_filename) + HEADER_LEN;
	uint8_t data[dataLen];
	uint32_t windowSizeNet = htonl(windowSize);
	uint16_t bufferSizeNet = htons(bufferSize);
	memcpy(data, &windowSizeNet, 4);
	memcpy(data + 4, &bufferSizeNet, 2);
	memcpy(data + 6, from_filename, strlen(from_filename) + 1);
	uint8_t *filePDU = buildPDU(data, dataLen, 0, FLAG_FILE_REQ); //build filename PDU
	safeSendto(socketNum, filePDU, dataLen + HEADER_LEN, 0, (struct sockaddr *)serverAddress, addressLen); //send filename PDU
	(*seqNum)++; //increment number of packets sent
	(*requestCount)++; //increment count of file request packets sent
	return SETUP_REPLY;
}

/* Get file request ack or resend filename packet */
STATE transferReply(int socketNum, struct sockaddr_in6 *server, uint32_t *requestCount, uint32_t *seqNum, char *from_filename, int *addrLen){
	int timerExpired = timer();
	if (!timerExpired){ //filename response received
		uint8_t buffer[MAX_PDU];
		int pduLen = safeRecvfrom(socketNum, buffer, MAX_PDU, 0, (struct sockaddr *)server, addrLen);
		if (in_cksum((uint16_t *) buffer, pduLen) != 0) { //check if file is corrupted
			(*seqNum)++; //increment num packets sent
			(*requestCount)--;
			if (*requestCount > 9) {
				printf("Server closed\n");
				return END;
			}
			else {
				return SETUP_REQ;
			}
		}
		struct pdu *received = (struct pdu *)buffer; //extract pdu info
		if(received->flag == FLAG_FILE_RES){
			if(received->payload[0]){//received file Ack
				return IN_ORDER;
			} else { //received file Nack
				printf("Error: file %s not found\n", from_filename);
				return END;
			}
		}
	}
	if (*requestCount == 10){printf("Server Closed\n"); return END;} //server is unresponsive
	return SETUP_REQ;
}

/* In-Order State */
STATE receiveData(int socketNum, struct sockaddr_in6* serverAddress, int toFD, uint16_t buffSize, uint32_t *expectedSeqNum, uint32_t *highestRRSREJ, uint32_t *sequenceNum, int *addrLen){
	printf("in-order\n");
	int timerExpired = timer(); 
	if(timerExpired) return END; //server is unresponsive

	uint8_t buffer[buffSize + HEADER_LEN]; //buffer for receiving data
	int pduLen = safeRecvfrom(socketNum, buffer, buffSize + HEADER_LEN, 0, (struct sockaddr *)serverAddress, addrLen);
	struct pdu *received = (struct pdu *)buffer;
	uint32_t seqNumRecv = ntohl(received->sequenceNum);

	if(received->flag == FLAG_EOF){ //if EOF packet received
		return TEARDOWN;
	} else if(in_cksum((uint16_t *) buffer, pduLen) == 0){ //verify no corruption
		if(seqNumRecv > *expectedSeqNum){ //if behind buffer data and SREJ missing data
			sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_SREJ);
			*highestRRSREJ = seqNumRecv;
			addBuffVal(packetBuff, buffer, buffSize + HEADER_LEN, seqNumRecv);
			return BUFFERING;
		} else if(seqNumRecv == *expectedSeqNum){ //in order
			write(toFD, received->payload, pduLen - HEADER_LEN);
			*highestRRSREJ = *expectedSeqNum;
			(*expectedSeqNum)++;
			sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_RR);
		} else{ //if ahead resend RR
			sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_RR);
		}
	}
	return IN_ORDER;
}

/* Sends final packet and closes communication */
STATE sendEOFack(int socketNum, uint32_t *sequenceNum, int toFD, struct sockaddr_in6 *serverAddress, int *addrLen){
	uint8_t placeholder = 0;
	uint8_t *EOFack = buildPDU((uint8_t *)&placeholder, 1, *sequenceNum, FLAG_EOF_ACK);
	safeSendto(socketNum, EOFack, HEADER_LEN + 1, 0, (struct sockaddr *)serverAddress, *addrLen);
	(*sequenceNum)++;
	// if(timer()) printf("Server Closed after EOF\n"); return END;
	// safeRecvfrom(socketNum, EOFack, HEADER_LEN + 1, 0, (struct sockaddr *)serverAddress, addrLen);
	// if (EOFack[4] == FLAG_EOF_ACK) return END;
	return END;
}

/* Helper function for sending RR and SREJ */
void sendRRSREJ(int socketNum, struct sockaddr_in6* serverAddress, uint32_t *sequenceNum, uint32_t *RRSREJnum, int flag){
	uint32_t RRSREJnum_Net = htonl(*RRSREJnum); //convert RR/SREJ sequence number to network order
	uint8_t RRSREJdata[4]; //sequence number RR/SREJ'd
	memcpy(RRSREJdata, &RRSREJnum_Net, 4);
	uint8_t *RRSREJ = buildPDU(RRSREJdata, 4, *sequenceNum, flag); 
	safeSendto(socketNum, RRSREJ, RRSREJ_LEN, 0, (struct sockaddr *)serverAddress, (int) sizeof(struct sockaddr_in6));
	(*sequenceNum)++; //increment number of packets sent
}

/* Buffer State*/
STATE bufferData(int socketNum, struct sockaddr_in6* serverAddress, int toFD, uint16_t buffSize, uint32_t *expectedSeqNum, uint32_t *highestRRSREJ, uint32_t *sequenceNum, int *addrLen){
	printf("buffering\n");
	int timerExpired = timer();
	if(timerExpired) return END; //server unresponsive

	uint8_t buffer[buffSize + HEADER_LEN];
	int pduLen = safeRecvfrom(socketNum, buffer, buffSize + HEADER_LEN, 0, (struct sockaddr *)serverAddress, addrLen); //receive data from server
	struct pdu *received = (struct pdu *)buffer;
	uint32_t seqNumRecv = ntohl(received->sequenceNum);
	if(in_cksum((uint16_t *) buffer, pduLen) == 0){ //check if corrupted
		if(seqNumRecv > *expectedSeqNum){ //if behind, buffer data
			*highestRRSREJ = seqNumRecv;
			addBuffVal(packetBuff, buffer, buffSize + HEADER_LEN, seqNumRecv);
		} else if(seqNumRecv == *expectedSeqNum){ //if receive expected start flushing the buffer
			write(toFD, received->payload, pduLen - HEADER_LEN);
			(*expectedSeqNum)++;
			return FLUSHING;
		} else{ //if ahead send RR
			sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_RR);
		}
	}
	return BUFFERING;
}

/* Flushing Sate */
STATE flushData(int socketNum, struct sockaddr_in6* serverAddress, int toFD, uint16_t buffSize, uint32_t *expectedSeqNum, uint32_t *highestRRSREJ, uint32_t *sequenceNum){
	printf("flushing\n");
	while((*expectedSeqNum < *highestRRSREJ) && validityCheck(packetBuff, *expectedSeqNum)){ //flush all valid data in buffer
		BufferVal flushVal = getBuffVal(packetBuff, *expectedSeqNum);
		write(toFD, flushVal.PDU + HEADER_LEN, flushVal.dataLen); //write to output file
		setInvalid(packetBuff, *expectedSeqNum); //clear buffer
		(*expectedSeqNum)++;
	}
	if (*expectedSeqNum == *highestRRSREJ) { //buffer empty
		sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_RR);
		return IN_ORDER;
	} else { //missing another
		sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_RR);
		sendRRSREJ(socketNum, serverAddress, sequenceNum, expectedSeqNum, FLAG_SREJ);
	}
	return BUFFERING;
}

/* Timer for server timeout */
int timer(){ 
	int testSocket = 0;
	if (!((testSocket = pollCall(10000)) > 0)) { //timer expired
		printf("Server Closed\n");
		return 1;
	}
	return 0;
}

int checkArgs(int argc, char * argv[]){
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
