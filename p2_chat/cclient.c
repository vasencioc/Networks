// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//
// Client setup and control for chat

#include "pollLib.h"
#include "networks.h"
#include "safeUtil.h"
#include "chatHelpers.h"
#include "HandleTable.h"

/*** Function Prototypes ***/
void serverClosed(int socket);
void login(char *handle, int socketNum);
void clientControl(char *handle, int socketNum);
void processMsgFromServer(char *handle, int socketNum);
void processStdin(char *handle, int socketNum);
void checkArgs(int argc, char * argv[]);
void printPrompt();
void displayText(uint8_t *packet, uint8_t flag);
void getNumHandles(char * handle, int socketNum, uint8_t *packet);
void getHandlesList(char * handle, int socketNum, uint8_t *packet);
void buildHdr(uint8_t flag, char *clientHandle, uint8_t *buffer);
int getTxt(uint8_t *buffer, uint16_t *inputLen, char *aChar);
void sendTxt(int socketNum, uint8_t *buffer, uint16_t *inputLen);
uint8_t packDestHandles(uint16_t *inputLen, uint8_t numDest, char *clientHandle, uint8_t *buffer);
void messagePacket(uint8_t flag, char *clientHandle, int socketNum);
void displayError(uint8_t *buffer);

/*** Source Code ***/
int main(int argc, char * argv[]){
	int socketNum = 0;         //socket descriptor
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
    login(argv[1], socketNum);
	// for(int i = 0; i < 200; i++){
	// 	char handleName[250] = {0};
	// 	sprintf(handleName, "test%d", i);
	// 	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	// 	login(handleName, socketNum);
	// }

	// while(1) {};

	// return 0;
	clientControl(argv[1], socketNum);
	close(socketNum);
	return 0;
}

/*Helper for printing stdin prompt*/
void printPrompt(){
	printf("$: ");
	fflush(stdout);
}

/*Clean-up when server connection closed*/
void serverClosed(int socket){
	close(socket);
    removeFromPollSet(socket);
	printf("\nServer Terminated\n");
    exit(1);
}

/* Logs client into server if the connection is valid and the handle is unique */
void login(char * handle, int socketNum){
	uint8_t packetLen = strlen(handle) + 2; //add space for flag and length field
	if(strlen(handle) > MAXHANDLE){
		printf("Invalid handle, handle longer than 100 characters: %s\n", handle);
		close(socketNum);
		exit(-1);
	}
	uint8_t flag1 = FLAG1; //flag 1 for login
	uint8_t *packed_handle = packHandle(handle); //get packed handle
	uint8_t handlePlusFlag[packetLen]; //new buffer with space for flag
	memcpy(handlePlusFlag, &flag1, 1); 	//packed handle preceded by flag
	memcpy(handlePlusFlag + 1, packed_handle, packed_handle[0] + 1);
	int sent = sendPDU(socketNum, handlePlusFlag, packetLen);
	//check if sent properly
	if(sent <= 0) serverClosed(socketNum);

	//get server response
	processMsgFromServer(handle, socketNum);
}

/*Controls client communication from server and stdin*/
void clientControl(char *handle, int socketNum){
	//polling setup
	setupPollSet();
    addToPollSet(socketNum);
	//add stdin to pollset for client input
    addToPollSet(STDIN_FILENO);
	int readySocket;
	printPrompt();
	while(1){
		readySocket = pollCall(-1); //poll until a socket is ready
		if(readySocket == STDIN_FILENO) processStdin(handle, socketNum);
		else if(readySocket == socketNum) processMsgFromServer(handle, readySocket);
		else{
			close(socketNum);
			perror("poll timeout");
			exit(1);
		}
	}
}

/*Display messages from other clients via the server*/
void displayText(uint8_t *packet, uint8_t flag){
	int offset = 1; //skip flag
	char *srcHandle = unpackHandle(packet + offset);
	uint8_t srcHandleLen = (uint8_t)packet[offset];
	offset += (srcHandleLen + 1); //move past srcHandle
	if((flag == FLAG5) || (flag == FLAG6)){
		uint8_t numDest = (uint8_t)packet[offset];
		offset++; //move past num dest 
		for(uint8_t i = 0; i < numDest; i++){
			uint8_t destHandleLen = (uint8_t)packet[offset];
			offset += destHandleLen + 1; //move past destHandle
		}
	}
	// at this point valuePtr should point to the beginning of the text message
	printf("\n%s: %s\n", srcHandle, packet + offset);
	printPrompt();
}

/*For %L displaying number of handles, after receiving flag 11*/
void getNumHandles(char * handle, int socketNum, uint8_t *packet){
	removeFromPollSet(STDIN_FILENO);
	uint32_t numNetOrdr, numHostOrdr;
	memcpy(&numNetOrdr, packet + 1, 4);
	numHostOrdr = ntohl(numNetOrdr);
	printf("Number of clients: %d\n", numHostOrdr);
}

/*For %L displaying handle names, after receiving flag 12*/
void getHandlesList(char * handle, int socketNum, uint8_t *packet){
	char *handleName = unpackHandle(packet + 1);
	printf("   %s\n", handleName);
}

/*Print error message for invalid handles*/
void displayError(uint8_t *buffer) {
    char *invalidHandle = unpackHandle(buffer + 1);
    printf("Client with handle %s does not exist.\n", invalidHandle);
	printPrompt();
}

/*Handles packets sent from the server*/
void processMsgFromServer(char *handle, int socketNum){
    uint8_t dataBuffer[MAXPACKET];
	int messageLen = 0;
	//now get the data from the server socket
	if((messageLen = recvPDU(socketNum, dataBuffer, MAXPACKET)) > 0){
		//printf("\nMessage received on socket: %d, length: %d Data: %s\n", socketNum, messageLen, dataBuffer + 2);
		uint8_t flag = dataBuffer[0];
		switch(flag) {
			case(FLAG2): break;
			case(FLAG3): printf("Handle already in use: %s\n", handle); exit(-1);
			case(FLAG4): displayText(dataBuffer, flag); break;
			case(FLAG5): displayText(dataBuffer, flag); break;
			case(FLAG6): displayText(dataBuffer, flag); break;
			case(FLAG7): displayError(dataBuffer); break;
			case(FLAG11): getNumHandles(handle, socketNum, dataBuffer); break;
			case(FLAG12): getHandlesList(handle, socketNum, dataBuffer); break;
			case(FLAG13): {
				printPrompt();
				addToPollSet(STDIN_FILENO);
				break;
			}
			default: break;
		}
	}
	//clean up if connection closed
	else{
        serverClosed(socketNum);
	}
}

/*Helper for sending text messages
 * builds header with flag and client handle */
void buildHdr(uint8_t flag, char *clientHandle, uint8_t *buffer){
	uint8_t *clientHandlePacked = packHandle(clientHandle);
	buffer[0] = flag;	// to complete chat header
	memcpy(buffer + 1, clientHandlePacked, strlen(clientHandle) + 1); //packet starts w/ packed src handle
}

/*Grabs message text from stdin and adds to packet*/
int getTxt(uint8_t *buffer, uint16_t *inputLen, char *aChar){
	int textLen = 0;
	while ((*inputLen < (MAXPACKET - 1)) && (*aChar != '\n') && (textLen < MAXTEXT))
	{
		*aChar = getchar();
		if (*aChar != '\n')
		{	
			buffer[*inputLen] = *aChar;
			(*inputLen)++;
			textLen++;
		}
	}
	return textLen;
}

/*Sends message packet to server*/
void sendTxt(int socketNum, uint8_t *buffer, uint16_t *inputLen){
	buffer[(*inputLen)] = '\0'; // null terminate the string
	(*inputLen)++;
	int sent = sendPDU(socketNum, buffer, *inputLen);
	if(sent <= 0) serverClosed(socketNum);
}

/*Helper for packing destination handles into a message packet
 * Returns a flag if one of the destination handles is the source handle*/
uint8_t packDestHandles(uint16_t *inputLen, uint8_t numDest, char *clientHandle, uint8_t *buffer){
	char aChar, destHandle[MAXHANDLE + 1];
	uint8_t sendToSelf = 0;
	buffer[(*inputLen)] = numDest;
	(*inputLen)++;
	for(uint8_t i = 0; i < numDest; i++){
		uint8_t destHandleLen = 0;
		aChar = getchar();
		while(aChar != ' ' && aChar != '\n'){
			destHandle[destHandleLen] = aChar;
			destHandleLen++;
			aChar = getchar();
		}
		destHandle[destHandleLen] = '\0'; //null terminate handle name
		if(strcmp(destHandle, clientHandle) == 0) sendToSelf = 1;
		destHandleLen++;
		uint8_t * destHandlePacked = packHandle(destHandle);
		//add packed dest handle to packet
		memcpy(buffer + (*inputLen), destHandlePacked, destHandleLen);
		*inputLen += destHandleLen;
	}
	return sendToSelf;
}

/*Creates a packet for sending text messages*/
void messagePacket(uint8_t flag, char *clientHandle, int socketNum){
	uint8_t sendToSelf = 0, numDest = 0, buffer[MAXPACKET]; 
	memset(buffer, 0, MAXPACKET);
	char aChar = 0;
	uint16_t inputLen = 0;        
	//build header with client handle
	buildHdr(flag, clientHandle, buffer);
	inputLen = strlen(clientHandle) + 2; // add packed header len and flag to packet len
	if(getchar() != ' '){printf("Invalid command format\n");}
	//add number of destinations to packet
	if(flag == FLAG5){
		numDest = 1;
	} else if (flag == FLAG6){
		numDest = getchar() - '0'; //expect num destination handles
		if((numDest > 9) || (numDest < 2)) {printf("Number of destination clients must be 2-9\n"); return;}
		if(getchar() != ' ') {printf("Invalid command format\n"); return;}
	}
	if(numDest != 0){
		sendToSelf = packDestHandles(&inputLen, numDest, clientHandle, buffer);
	}
	uint8_t *messageStart = buffer + inputLen; //new pointer for start of text
	int firstChunkSent = 0;
	// loop until the full message is sent
	while(1) {
		int textLen = getTxt(buffer, &inputLen, &aChar); //get text from stdin
		sendTxt(socketNum, buffer, &inputLen); //send message
		if (!firstChunkSent && sendToSelf) {
			processMsgFromServer(clientHandle, socketNum); //get message sent to self
			firstChunkSent = 1;
		}
		if ((aChar == '\n') || (textLen < MAXTEXT)) break;
		inputLen = messageStart - buffer;
	}
}

/*Process client input from stdin*/
void processStdin(char *handle, int socketNum){
	char command = 0;
	while(command != '%'){
		command = getchar();
	}
	command = getchar();
	switch(tolower(command)){
		case('m'): {
			messagePacket(FLAG5, handle, socketNum); 
			printPrompt();
			break;
		}
		case('c'): {
			messagePacket(FLAG6, handle, socketNum); 
			printPrompt();
			break;
		}
		case('b'): {
			messagePacket(FLAG4, handle, socketNum);
			printPrompt();
			break;
		}
		case('l'): sendFlag(socketNum, FLAG10); break;
		default: printf("Invalid command\n"); break;
	}
}

/* Check for correct input*/
void checkArgs(int argc, char * argv[]){
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle-name host-name port-number \n", argv[0]);
		exit(1);
	}
}
