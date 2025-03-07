// 
// Writen by Victoria Asencio-Clemens, March 2025
//
// Server setup and control for rcopy

#include "poll.h"
#include "networks.h"
#include "safeUtil.h"
#include "windowLib.h"
#include "PDU.h"

/*** States for FSM ***/
typedef enum {
	SETUP,
	USE,
	TEARDOWN,
	END
} STATE;

/*** Function Prototypes ***/
void processClient(int mainServerSocket);
void fileTransfer(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, uint8_t *pdu);
STATE sendSetup(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, struct pdu PDU, char *fileName, int *fromFD);
STATE sendEOF(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, uint32_t* seqNum);
STATE sendData(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, uint32_t sequenceNum, int *fromFD);
void processRRSREJ(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen);
int checkArgs(int argc, char *argv[]);

/*** Source Code ***/

WindowBuff* window;

int main ( int argc, char *argv[]  )
{ 
	int socketNum = 0;				
	int portNumber = 0;

	portNumber = checkArgs(argc, argv);
		
	socketNum = udpServerSetup(portNumber);

	processClient(socketNum);

	close(socketNum);
	
	return 0;
}

void handleZombies(int sig) {
	int stat = 0;
	while (waitpid(-1, &stat, WNOHANG) > 0)
	{}
}

/*Main control for polling sockets*/
void processClient(int mainServerSocket){
	int dataLen = 0; 
	char buffer[MAX_PDU];	  
	struct sockaddr_in6 client;		
	int clientAddrLen = sizeof(client);
	buffer[0] = '\0';
	//control loop
	//while(1){ //ADD BACK WHEN FORKING
	    dataLen = safeRecvfrom(mainServerSocket, buffer, MAXBUF, 0, (struct sockaddr *) &client, &clientAddrLen);
		if(dataLen != 0){ //replace with fork later
			fileTransfer(mainServerSocket, &client_address, client_len, initialPDU);
		}
	//}
}

/* Control client communication*/
void fileTransfer(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, uint8_t *pdu){
	window = createWindow(windowLen, bufferLen);
	// setup variables for sending data
	struct pdu *received = (struct pdu *)pdu; //initial pdu
	char from_filename[MAX_FILENAME + 1]; //add space for null-terminator
	int fromFD;
	uint32_t sequenceNum = 0;
	// setup poll set for receiving data
	setupPollSet();
	addToPollSet(socketNum);
	// FSM for flow control
	STATE presentState = SETUP;
	while(presentState != END){
		switch(presentState) {
			case SETUP: {
				if(received->flag == FLAG_FILE_REQ){
					presentState = sendSetup(socketNum, clientAddress, clientLen, received, from_filename, &fromFD);
				} else{
					presentState = END;
				}
				break;
			}
			case USE: presentState = sendData(socketNum, clientAddress, clientLen, sequenceNum, fromFD);
			case TEARDOWN: presentState = sendEOF(socketNum, clientAddress, clientLen, &sequenceNum);
			case END: break;
			default: break;
		}
	}
	//clean up
	destroyWindow(window);
	removeFromPollSet(socketNum);
	close(fromFD);
	clientClosed(socketNum);
}

STATE sendSetup(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, struct pdu PDU, uint32_t *seqNum char *fileName, int *fromFD){
	STATE nextState;
	uint8_t response;
	uint32_t windowSize;
	uint32_t bufferSize;
	memcpy(windowSize, PDU.payload[0], 4);
	memcpy(bufferSize, PDU.payload[4], 2);
	memcpy(fileName, PDU.payload[6], MAX_FILENAME);
	fromFD = open(fileName, O_RDONLY); //attempt to open from file as read-only
	if(fromFD < 0){ // unable to open file
		nextState = END;
		response = 0;
	} else {
		nextState = USE;
		response = 1;
	}
	char *sendPDU = buildPDU(response, 1, *seqNum, FLAG_FILE_RES);
	safeSendTo(socketNum, sendPDU, sizeof(sendPDU), 0, clientAddress, clientLen);
	(*sequenceNum)++;
	return nextState;
}

STATE sendEOF(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, uint32_t* seqNum){
	uint8_t placeholder = 0;
	uint8_t *EOF = buildPDU(placeholder, 1, seqNum, FLAG_EOF);
	int count = 0;
	int fromSocket;
	while(count < 10){
		safeSendTo(socketNum, EOF, sizeof(EOF), 0, clientAddress, clientLen); //send EOF
		if(int fromSocket = poll(1000)){ //1 sec timer to wait for EOF ack
			uint8_t EOFack[HEADER_LEN + 1];
			int pduLen = safeRecvFrom(socketNum, EOFack, HEADER_LEN + 1, 0, (struct sockaddr *)&client_address, &client_len);
			if (in_cksum((uint16_t *) EOFack, pduLen) == 0) {
				if (finalPDU[7] != FLAG_EOF_ACK) { //PROGRAM FINISH
					uint8_t *close = buildPDU(placeholder, 1, seqNum, FLAG_EOF_ACK);
					safeSendto(socketNum, close, sizeof(close), 0, clientAddress, clientLen); //send final ACK
					return END;
				}
			}
		}
		count++;
	}
	if(fromSocket){
		uint8_t finalPDU[HEADER_LEN];
		int pduLen = safeRecvFrom(socketNum, finalPDU, HEADER_LEN, 0, (struct sockaddr *)&client_address, &client_len);
		if(finalPDU[7] != FLAG_EOF_ACK){
			recvError = 1
		}
	}
	printf("Error Sending EOF");
	return END;
}

STATE sendData(socketNum, clientAddress, clientLen, sequenceNum, fromFD, bufferLen, windowLen){
	int reachedEnd = 0; //flag for end of file
	uint32_t trackSeqNum = 0;
	uint32_t lastSeqNum = 0;
	uint8_t *buffer[bufferLen];
	while(!reachedEnd){
		while(!windowCheck(window)){ //window open
			int lenRead = read(fromFD, buffer, bufferLen); //read data
			if(lenRead == 0){
				lastSeqNum = *sequenceNum;
				reachedEnd = 1;
				break;
			}
			uint8_t *pdu = buildPDU(buffer, bufferLen, sequenceNum, FLAG_DATA); //create PDU
			addVal(window, pdu, lenRead + 7, sequenceNum); //store PDU
			safeSendTo(socketNum, pdu, lenRead + 7, 0, clientAddress, clientLen); //send data
			while(pollCall(0)){
				processRRSREJ(socketNum, clientAddress, clientLen, &trackSeqNum);
			}
		}
		uint32_t count = 0;
		while(windowCheck(window)){ //window closed
			if(pollCall(1000)){
				processRRSREJ(socketNum, clientAddress, clientLen, &trackSeqNum);
			} else{
				//resend lowest packet
				WindowVal *lowest = getVal(window, window->lower);
				uint8_t *resend[lowest.dataLen + 7];
				memcpy(resend, lowest.PDU, lowest.dataLen + 7);
				safeSendTo(socketNum, resend, lowest.dataLen + 7, 0, clientAddress, clientLen);
				//increment count of resends
				count++;
				if(count == 10){
					close(fromFD);
					return END;
				}
			}
		}
	}
}

void processRRSREJ(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, uint32_t *RRnum){
	uint8_t *pdu[RRSREJ_LEN];
	int pduLen = safeRecvFrom(sockerNum, pdu, RRSREJ_LEN, 0, clientAddress, clientLen);
	if (in_cksum((uint16_t *) pdu, RRSREJ_LEN) == 0) { 
		struct pdu *received = (struct pdu *)pdu;
		uint32_t seqNumResponse;
		memcpy(seqNumResponse, received.payload + 7, 4);
		if(received.flag == FLAG_RR){
			RRnum= ntohl(seqNumResponse);
			slideWindow(window, RRnum);
		} else if(received.flag == FLAG_SREJ){
			//resend rejected pdu
			WindowVal SREJval = getVal(window, seqNumResponse);
			safeSendTo(socketNum, SREJval.PDU, SREJval.dataLen + 7, 0, clientAddress, clientLen); //send data
		}
	}
}

/*Check command line arguments for proper login*/
int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(1);
	}

	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}

	return portNumber;
}
