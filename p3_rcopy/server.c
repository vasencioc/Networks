// 
// Writen by Victoria Asencio-Clemens, March 2025
//
// Server setup and control for rcopy

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
void handleZombies(int sig);
void processClient(double error_rate, int mainServerSocket);
void fileTransfer(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint8_t *pdu);
STATE sendSetup(int socketNum, struct sockaddr_in6 *clientAddress, struct pdu PDU, int clientLen, uint32_t *seqNum, char *fileName,int *fromFD, uint16_t *bufferLen);
STATE sendEOF(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t* seqNum);
STATE download(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t *sequenceNum, int fromFD, uint16_t bufferLen);
int catchUp(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t *lastRR, uint32_t *count);
void processRRSREJ(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t *RRnum);
void resendLowest(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t *count);
int checkArgs(int argc, char *argv[]);

/*** Source Code ***/
WindowBuff* window; //windowing buffer

/* Program Entry Point */
int main ( int argc, char *argv[]){ 
	// inititalize variables for UDP communication
	int socketNum = 0;				
	int portNumber = 0;
	portNumber = checkArgs(argc, argv);
	double error_rate = atof(argv[1]);

	handleZombies(SIGCHLD); //handler for zombie processes

	socketNum = udpServerSetup(portNumber);
	processClient(error_rate, socketNum);

	close(socketNum);
	return 0;
}

/* Handler for Zombie Processes */
void handleZombies(int sig) {
	int stat = 0;
	while (waitpid(-1, &stat, WNOHANG) > 0){}
}

/* Receive client communication and fork to handle multiple clients */
void processClient(double error_rate, int mainServerSocket){
	int dataLen = 0; 
	uint8_t buffer[MAX_PDU];	  
	struct sockaddr_in6 client;		
	int clientAddrLen = sizeof(client);
	//buffer[0] = '\0';
	pid_t pid = 0;
	
	while(1){
		//receive filename packet from client
	    dataLen = safeRecvfrom(mainServerSocket, buffer, MAX_PDU, 0, (struct sockaddr *)&client, &clientAddrLen);
		if(in_cksum((uint16_t *)buffer, dataLen) == 0){ //check for corruption
			if((pid = fork()) == 0){ //child process
				sendtoErr_init(error_rate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
				fileTransfer(mainServerSocket, &client, clientAddrLen, buffer); //start file download
				exit(0);
			}
		}
	}
}

/* Control communication to/from client */
void fileTransfer(int mainServerSocket, struct sockaddr_in6 *clientAddress, int clientLen, uint8_t *pdu){
	close(mainServerSocket);
	int socketNum = 0;
	// create new socket
	if ((socketNum = socket(AF_INET6,SOCK_DGRAM,0)) < 0)
	{
		perror("socket() call error");
		exit(-1);
	}
	// setup variables for sending data
	struct pdu *received = (struct pdu *)pdu; //initial pdu
	char from_filename[MAX_FILENAME + 1]; //add space for null-terminator
	int fromFD;
	uint32_t sequenceNum = 0;
	uint16_t bufferLen = 0;
	// setup poll set for receiving data
	setupPollSet();
	addToPollSet(socketNum);
	// FSM for flow control
	STATE presentState = SETUP;
	while(presentState != END){
		switch(presentState) {
			case SETUP: {
				if(received->flag == FLAG_FILE_REQ){ //first packet must be file request 
					presentState = sendSetup(socketNum, clientAddress, *received, clientLen, &sequenceNum, from_filename, &fromFD, &bufferLen);
				} else{presentState = END;}
				break;	
			}							
			case USE: presentState = download(socketNum, clientAddress, clientLen, &sequenceNum, fromFD, bufferLen); break;
			case TEARDOWN: presentState = sendEOF(socketNum, clientAddress, clientLen, &sequenceNum); break;
			case END: break;
			default: break;
		}
	}
	//clean up
	destroyWindow(window);
	removeFromPollSet(socketNum);
	close(fromFD);
}

/* Setup State. Send File Request Ack*/
STATE sendSetup(int socketNum, struct sockaddr_in6 *clientAddress, struct pdu PDU, int clientLen, uint32_t *seqNum, char *fileName,int *fromFD, uint16_t *bufferLen){
	STATE nextState;
	uint8_t response[1] = {0};
	uint32_t windowSize;
	uint32_t windowSizeNet;
	uint16_t bufferSizeNet;
	//fileName[MAX_FILENAME];
	memcpy(&windowSizeNet, PDU.payload, 4);
	memcpy(&bufferSizeNet, PDU.payload + 4, 2);
	memcpy(fileName, PDU.payload + 6, MAX_FILENAME + 1);
	fileName[MAX_FILENAME] = '\0';
	windowSize = ntohl(windowSizeNet);
	*bufferLen = ntohs(bufferSizeNet);

	window = createWindow(windowSize, *bufferLen); //build window buffer, global variable
	*fromFD = open(fileName, O_RDONLY); //attempt to open from file as read-only
	if(*fromFD < 0){ // unable to open file
		nextState = END;
		response[0] = 0; //nack
	} else { // file opened successfully, go to use state
		nextState = USE;
		response[0] = 1; //ack
	}
	// send file request ack/nack
	uint8_t *sendPDU = buildPDU(response, 1, *seqNum, FLAG_FILE_RES);
	safeSendto(socketNum, sendPDU, 1 + HEADER_LEN, 0, (struct sockaddr *)clientAddress, clientLen);
	(*seqNum)++;
	return nextState;
}

/* Teardown State. Send EOF packet */
STATE sendEOF(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t* seqNum){
	uint8_t placeholder = 0;
	uint8_t *EOFpacket = buildPDU(&placeholder, 1, *seqNum, FLAG_EOF);
	int count = 0;
	while(count < 10){
		safeSendto(socketNum, EOFpacket, HEADER_LEN + 1, 0, (struct sockaddr *)clientAddress, clientLen); //send EOF
		int sock = 0;
		if ((sock = pollCall(1000)) > 0){ //1 sec timer to wait for EOF ack
			uint8_t EOFack[HEADER_LEN + 1];
			int pduLen = safeRecvfrom(sock, EOFack, HEADER_LEN + 1, 0, (struct sockaddr *)clientAddress, &clientLen);
			if (in_cksum((uint16_t *) EOFack, pduLen) == 0) {
				if (EOFack[6] == FLAG_EOF_ACK) { 
					//uint8_t *close = buildPDU(placeholder, 1, seqNum, FLAG_EOF_ACK);
					//safeSendto(socketNum, close, sizeof(close), 0, clientAddress, clientLen); //send final ACK
					return END;
				}
			}
		}
		count++;
	}
	// if(fromSocket){
	// 	uint8_t finalPDU[HEADER_LEN];
	// 	int pduLen = safeRecvfrom(socketNum, finalPDU, HEADER_LEN, 0, (struct sockaddr *)&client_address, &client_len);
	// 	if(finalPDU[7] != FLAG_EOF_ACK){
	// 		recvError = 1
	// 	}
	// }
	printf("Error Sending EOF");
	return END;
}

/* Use State */
STATE download(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t *sequenceNum, int fromFD, uint16_t bufferLen){
	uint8_t buffer[bufferLen];
	memset(buffer, 0, bufferLen + HEADER_LEN);
	int reachedEnd = 0; //flag for end of file
	uint32_t count; //resend count
	uint32_t receivedRR = 0; //
	uint32_t prevSeqNum = 0;

	slideWindow(window, *sequenceNum);
	window->current = *sequenceNum;

	while(!reachedEnd){
		while(!windowCheck(window)){ //window open
			int lenRead = read(fromFD, buffer, bufferLen); //read data
			if(lenRead == 0){
				prevSeqNum = *sequenceNum;
				reachedEnd = 1;
				break;
				//return TEARDOWN;
			}
			uint8_t *pdu = buildPDU(buffer, lenRead, *sequenceNum, FLAG_DATA); //create PDU
			safeSendto(socketNum, pdu, lenRead + HEADER_LEN, 0, (struct sockaddr *)clientAddress, clientLen); //send data
			addWinVal(window, pdu, lenRead + HEADER_LEN, *sequenceNum); //store PDU
			(*sequenceNum)++;
			int sock = 0;
			while ((sock = pollCall(0)) > 0){
				processRRSREJ(sock, clientAddress, clientLen, &receivedRR);
			}
		}
		count = 0;
		while(windowCheck(window)){ //window closed
			printf("window closed\n");
			int sock = 0;
			if ((sock = pollCall(1000)) > 0){
				processRRSREJ(sock, clientAddress, clientLen, &receivedRR);
			} else{
				resendLowest(socketNum, clientAddress, clientLen, &count);
				if(count == 10){
					close(fromFD);
					return END;
				}
			}
		}
	}
	count = 0;//reset count 
	while(receivedRR != prevSeqNum){
		if(catchUp(socketNum, clientAddress, clientLen, &receivedRR, &count)){ return END;}
		// int sock = 0;
		// if ((sock = pollCall(1000)) > 0){
		// 	processRRSREJ(sock, clientAddress, clientLen, &receivedRR);
		// } else if(count == 10){
		// 	printf("Client Closed\n");
		// 	return END;
		// } else{
		// 	//resend lowest packet
		// 	WindowVal lowest = getWinVal(window, window->lower);
		// 	uint8_t resend[lowest.dataLen + 7];
		// 	memcpy(&resend, lowest.PDU, lowest.dataLen + 7);
		// 	safeSendto(socketNum, resend, lowest.dataLen + 7, 0, (struct sockaddr *)clientAddress, clientLen);
		// 	count++;
		// }
	}
	return TEARDOWN;
}

/*Process leftover RR/SREJ before sending EOF*/
int catchUp(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t *lastRR, uint32_t *count){
	int readySocket = 0;
	if ((readySocket = pollCall(1000)) > 0){
		processRRSREJ(readySocket, clientAddress, clientLen, lastRR);
	} else if(*count == 10){
		printf("Client Closed\n");
		return 1;
	} else{
		resendLowest(socketNum, clientAddress, clientLen, count); //resend lowest packet
	}
	return 0;
}

/* Receive RRs and SREJs */
void processRRSREJ(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t *RRnum){
	uint8_t pdu[RRSREJ_LEN];
	safeRecvfrom(socketNum, pdu, RRSREJ_LEN, 0, (struct sockaddr *)clientAddress, &clientLen);
	if (in_cksum((uint16_t *) pdu, RRSREJ_LEN) == 0) {  //check for corruption
		struct pdu *received = (struct pdu *)pdu;
		uint32_t seqNumResponse;
		memcpy(&seqNumResponse, received->payload, 4);
		seqNumResponse = ntohl(seqNumResponse);
		if(received->flag == FLAG_RR){ //RR received
			*RRnum = seqNumResponse;
			slideWindow(window, *RRnum);
		} else if(received->flag == FLAG_SREJ){ //SREJ received
			//resend rejected packet
			WindowVal SREJval = getWinVal(window, seqNumResponse);
			safeSendto(socketNum, SREJval.PDU, RRSREJ_LEN, 0, (struct sockaddr *)clientAddress, clientLen);
		}
	}
}

/* Resend lowest packet in the window buffer */
void resendLowest(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t *count){
	WindowVal lowest = getWinVal(window, window->lower); //get lowest value from window buffer
	uint8_t resend[lowest.dataLen + HEADER_LEN];
	memcpy(&resend, lowest.PDU, lowest.dataLen + HEADER_LEN);
	safeSendto(socketNum, resend, lowest.dataLen + HEADER_LEN, 0, (struct sockaddr *)clientAddress, clientLen);
	(*count)++; //increment count of resends
}

/*Check command line arguments for proper login*/
int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 3)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(1);
	}

	if (argc == 3)
	{
		portNumber = atoi(argv[2]);
	}

	return portNumber;
}
