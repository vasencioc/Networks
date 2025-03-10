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
STATE sendData(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t *sequenceNum, int fromFD, uint16_t bufferLen);
void processRRSREJ(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t *RRnum);
int checkArgs(int argc, char *argv[]);

/*** Source Code ***/

WindowBuff* window;

int main ( int argc, char *argv[]  )
{ 
	int socketNum = 0;				
	int portNumber = 0;
	portNumber = checkArgs(argc, argv);
	double error_rate = atof(argv[1]);

	handleZombies(SIGCHLD);

	socketNum = udpServerSetup(portNumber);

	processClient(error_rate, socketNum);

	close(socketNum);
	
	return 0;
}

void handleZombies(int sig) {
	int stat = 0;
	while (waitpid(-1, &stat, WNOHANG) > 0)
	{}
}

/*Main control for polling sockets*/
void processClient(double error_rate, int mainServerSocket){
	int dataLen = 0; 
	uint8_t buffer[MAX_PDU];	  
	struct sockaddr_in6 client;		
	int clientAddrLen = sizeof(client);
	buffer[0] = '\0';
	pid_t pid = 0;
	//control loop
	while(1){
	    dataLen = safeRecvfrom(mainServerSocket, buffer, MAX_PDU, 0, (struct sockaddr *)&client, &clientAddrLen);
		if(in_cksum((uint16_t *)buffer, dataLen) == 0){ 
			if((pid = fork()) == 0){ //child process
				sendtoErr_init(error_rate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
				fileTransfer(mainServerSocket, &client, clientAddrLen, buffer);
				exit(0);
			}
		}
	}
}

/* Control client communication*/
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
				if(received->flag == FLAG_FILE_REQ){
					presentState = sendSetup(socketNum, clientAddress, *received, clientLen, &sequenceNum, from_filename, &fromFD, &bufferLen);
				} else{
					presentState = END;
				}
				break;
			}								
			case USE: presentState = sendData(socketNum, clientAddress, clientLen, &sequenceNum, fromFD, bufferLen); break;
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

STATE sendSetup(int socketNum, struct sockaddr_in6 *clientAddress, struct pdu PDU, int clientLen, uint32_t *seqNum, char *fileName,int *fromFD, uint16_t *bufferLen){
	STATE nextState;
	uint8_t response[1] = {0};
	uint32_t windowSize;
	uint32_t windowSizeNet;
	uint16_t bufferSizeNet;
	memcpy(&windowSizeNet, PDU.payload, 4);
	windowSize = ntohl(windowSizeNet); // Convert to host order
	memcpy(&bufferSizeNet, PDU.payload + 4, 2);
	*bufferLen = ntohs(bufferSizeNet);
	memcpy(fileName, PDU.payload + 6, MAX_FILENAME + 1);
	fileName[MAX_FILENAME] = '\0';
	window = createWindow(windowSize, *bufferLen);
	*fromFD = open(fileName, O_RDONLY); //attempt to open from file as read-only
	if(*fromFD < 0){ // unable to open file
		nextState = END;
		response[0] = 0;
	} else {
		nextState = USE;
		response[0] = 1;
	}
	uint8_t *sendPDU = buildPDU(response, 1, *seqNum, FLAG_FILE_RES);
	safeSendto(socketNum, sendPDU, 1 + HEADER_LEN, 0, (struct sockaddr *)clientAddress, clientLen);
	(*seqNum)++;
	return nextState;
}

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
				if (EOFack[7] == FLAG_EOF_ACK) { 
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

STATE sendData(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t *sequenceNum, int fromFD, uint16_t bufferLen){

	int reachedEnd = 0; //flag for end of file
	uint32_t count;
	uint32_t trackSeqNum = 0;
	uint32_t lastSeqNum = 0;
	uint8_t buffer[bufferLen];
	memset(buffer, 0, bufferLen + HEADER_LEN);
	while(!reachedEnd){
		while(!windowCheck(window)){ //window open
			int lenRead = read(fromFD, buffer, bufferLen); //read data
			if(lenRead == 0){
				// lastSeqNum = *sequenceNum;
				// reachedEnd = 1;
				// break;
				return TEARDOWN;
			}
			uint8_t *pdu = buildPDU(buffer, lenRead, *sequenceNum, FLAG_DATA); //create PDU
			addWinVal(window, pdu, lenRead + HEADER_LEN, *sequenceNum); //store PDU
			safeSendto(socketNum, pdu, lenRead + HEADER_LEN, 0, (struct sockaddr *)clientAddress, clientLen); //send data
			(*sequenceNum)++;
			int sock = 0;
			while ((sock = pollCall(0)) > 0){
				processRRSREJ(sock, clientAddress, clientLen, &trackSeqNum);
			}
		}
		count = 0;
		while(windowCheck(window)){ //window closed
			int sock = 0;
			if ((sock = pollCall(1000)) > 0){
				processRRSREJ(sock, clientAddress, clientLen, &trackSeqNum);
			} else{
				//resend lowest packet
				WindowVal lowest = getWinVal(window, window->lower);
				uint8_t resend[lowest.dataLen + HEADER_LEN];
				memcpy(resend, lowest.PDU, lowest.dataLen + HEADER_LEN);
				safeSendto(socketNum, resend, lowest.dataLen + HEADER_LEN, 0, (struct sockaddr *)clientAddress, clientLen);
				//increment count of resends
				count++;
				if(count == 10){
					close(fromFD);
					return END;
				}
			}
		}
	}
	count = 0;//reset count 
	while(trackSeqNum != lastSeqNum){
		int sock = 0;
		if ((sock = pollCall(1000)) > 0){
			processRRSREJ(sock, clientAddress, clientLen, &trackSeqNum);
		} else if(count == 10){
			printf("Client Closed\n");
			return END;
		} else{
			//resend lowest packet
			WindowVal lowest = getWinVal(window, window->lower);
			uint8_t resend[lowest.dataLen + 7];
			memcpy(&resend, lowest.PDU, lowest.dataLen + 7);
			safeSendto(socketNum, resend, lowest.dataLen + 7, 0, (struct sockaddr *)clientAddress, clientLen);
			count++;
		}
	}
	return TEARDOWN;
}

void processRRSREJ(int socketNum, struct sockaddr_in6 *clientAddress, int clientLen, uint32_t *RRnum){
	uint8_t pdu[RRSREJ_LEN];
	safeRecvfrom(socketNum, pdu, RRSREJ_LEN, 0, (struct sockaddr *)clientAddress, &clientLen);
	if (in_cksum((uint16_t *) pdu, RRSREJ_LEN) == 0) { 
		struct pdu *received = (struct pdu *)pdu;
		uint32_t seqNumResponse;
		memcpy(&seqNumResponse, received->payload, 4);
		seqNumResponse = ntohl(seqNumResponse);
		if(received->flag == FLAG_RR){
			*RRnum = seqNumResponse;
			slideWindow(window, *RRnum);
		} else if(received->flag == FLAG_SREJ){
			//resend rejected pdu
			WindowVal SREJval = getWinVal(window, seqNumResponse);
			safeSendto(socketNum, SREJval.PDU, SREJval.dataLen + 7, 0, (struct sockaddr *)clientAddress, clientLen); //send data
		}
	}
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
