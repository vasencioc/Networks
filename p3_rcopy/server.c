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
void serverControl(int mainServerSocket);
void processClient(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, uint8_t *pdu);
STATE sendSetup(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, struct pdu PDU, char *fileName, int *fromFD);
STATE sendLast(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, uint32_t sequenceNum, uint8_t* lastPayload);
STATE sendData(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, uint32_t sequenceNum, int *fromFD);
void processRRSREJ(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen);
int checkArgs(int argc, char *argv[]);

/*** Source Code ***/
int main(int argc, char *argv[]) {
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = checkArgs(argc, argv);
	//create the server socket
	mainServerSocket = udpServerSetup(portNumber);
	//manage server communication
	serverControl(mainServerSocket);
	/* close the sockets */
	close(mainServerSocket);
	return 0;
}

/*Main control for polling sockets*/
void serverControl(int mainServerSocket){
	struct sockaddr_in6 client_address;
	socklen_t client_len = sizeof(client_address);
    //struct packet recv_pkt;
    int received;
	int flags = 0;
	//control loop
	//while(1){ //ADD BACK WHEN FORKING
		uint8_t *initialPDU[MAX_PDU];
		int pduLen = safeRecvFrom(mainServerSocket, (uint8_t *)initialPDU, MAX_PDU, 0, (struct sockaddr *)client_address, &client_len);
		if(pduLen != 0){ //replace with fork later
			processClient(mainServerSocket, &client_address, client_len, initialPDU);
		}
	//}
}

/* Control client communication*/
void processClient(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, uint8_t *pdu){
	// setup variables for sending data
	struct pdu *received = (struct pdu *)pdu; //initial pdu
	uint8_t *EOF[MAX_PAYLOAD]; //final payload
	char from_filename[MAX_FILENAME + 1]; //add space for null-terminator
	int fromFD;
	uint32_t sequenceNum = 0;
	// setup poll set for sending data
	setupPollSet();
	addToPollSet(socketNum);
	// FSM for flow control
	STATE presentState = SETUP;
	while(presentState != END)
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
		case TEARDOWN: presentState = sendLast(socketNum, clientAddress, clientLen, sequenceNum, EOF);
		case END: break;
		default: break;
	}
	//clean up
	removeFromPollSet(socketNum);
	clientClosed(socketNum);
}

STATE sendSetup(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, struct pdu PDU, char *fileName, int *fromFD){
	STATE nextState;
	uint8_t response;
	uint32_t windowSize;
	uint32_t bufferSize;
	memcpy(windowSize, PDU.payload[0], 4);
	memcpy(bufferSize, PDU.payload[4], 2);
	memcpy(fileName, PDU.payload[6], MAX_FILENAME);
	fromFD = open(fileName, O_RDONLY); //attempt to open from file as read-only
	if(fromFD < 0){ // unable to open file
		nextState = TEARDOWN;
		response = 0;
	} else {
		nextState = USE;
		response = 1;
	}
	char *sendPDU = buildPDU(response, 1, 0, FLAG_FILE_RES);
	safeSendTo(socketNum, sendPDU, sizeof(sendPDU), 0, clientAddress, clientLen);
	//POLL HERE AND SEND 10???????
	return nextState;
}

STATE sendLast(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen, uint8_t* lastPayload){
	char *sendPDU = buildPDU(lastPayload, 1, sequenceNum, FLAG_EOF);
	int count = 0;
	int recvError = 0; //flag for error receiving EOF Ack
	safeSendTo(socketNum, sendPDU, sizeof(sendPDU), 0, clientAddress, clientLen); //send EOF
	int fromSocket = poll(1000); //1 sec timer to wait for EOF ack
	while(fromSocket == -1 && count < 10){
		count++;
		safeSendTo(socketNum, sendPDU, sizeof(sendPDU), 0, clientAddress, clientLen); //resend EOF
		fromSocket = poll(1000); //1 sec timer to wait for EOF ack
	}
	if(fromSocket != -1){
		uint8_t finalPDU[MAX_PDU];
		int pduLen = safeRecvFrom(socketNum, finalPDU, MAX_PDU, flags, (struct sockaddr *)&client_address, &client_len);
		if(finalPDU[7] != FLAG_EOF_ACK){
			recvError = 1
		}
	}
	if(recvError || (count > 9)) printf("Error Sending EOF");
	close(fromFD);
	return DONE;
}

STATE sendData(socketNum, clientAddress, clientLen, sequenceNum, fromFD, bufferLen, windowLen){
	WindowBuff window = createWindow(windowLen, bufferLen);
	int reachedEnd = 0; //flag for end of file
	uint8_t *buffer[bufferLen];
	while(!reachedEnd){
		while(!windowCheck(window)){ //window open
			int lenRead = read(fromFD, buffer, bufferLen); //read data
			uint8_t *pdu = buildPDU(buffer, bufferLen, sequenceNum, FLAG_DATA); //create PDU
			addVal(window, pdu, lenRead + 7, sequenceNum); //store PDU
			safeSendTo(socketNum, pdu, lenRead + 7, 0, clientAddress, clientLen); //send data
			while(pollCall(0)){
				processRRSREJ(socketNum, clientAddress, clientLen);
			}
		}
		while(windowCheck(window)){ //window closed
			count = 0;
			if(pollCall(1000)){
				processRRSREJ(socketNum, clientAddress, clientLen);
			} else{
				//resend lowest packet
				WindowVal *lowest = getVal(window, 0);
				memcpy(buffer, lowest.PDU[7], lowest.dataLen);
				uint8_t *pdu = buildPDU(buffer, bufferLen, 0, FLAG_TIMEOUT_RES);
				//increment count of resends
				count++;
				if(count == 10){
					close(fromFD);
					return DONE;
				}
			}
		}
	}
}

void processRRSREJ(int socketNum, struct sockaddr_in6 *clientAddress, socklen_t clientLen){
	uint8_t *pduBuff[RRSREJ_LEN];
	int pduLen = safeRecvFrom(sockerNum, pduBuff, RRSREJ_LEN, 0, clientAddress, clientLen);
	struct pdu *received = (struct pdu *)pduBuff;
	uint32_t seqNumResponse;
	memcpy(seqNumResponse, received.payload, 4);
	if(received.flag == FLAG_RR){
		slideWindow(window, seqNumResponse);
	} else if(received.flag == FLAG_SREJ){
		//resend rejected pdu
		WindowVal SREJval = getVal(window, seqNumResponse);
		safeSendTo(socketNum, SREJval.PDU, SREJval.dataLen + 7, 0, clientAddress, clientLen); //send data
	}
}

/*Check command line arguments for proper login*/
int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 7)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(1);
	}

	if (argc == 7)
	{
		portNumber = atoi(argv[1]);
	}

	return portNumber;
}
