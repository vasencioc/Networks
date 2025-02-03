// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//
// Client setup and control for chat

#include "pollLib.h"
#include "networks.h"
#include "safeUtil.h"
#include "chatHelpers.h"
#include "HandleTable.h"

#define DEBUG_FLAG 1
#define CHATHEADER_SIZE 3
#define PDULEN_SIZE 2
#define FLAG_SIZE 1

#define FLAG1 1
#define FLAG2 2
#define FLAG3 3

void serverClosed(int socket);
void login(char *handle, int socketNum);
void clientControl(char *handle, int socketNum);
void processMsgFromServer(char *handle, int socketNum);
void processStdin(char *handle, int socketNum);
void checkArgs(int argc, char * argv[]);

int main(int argc, char * argv[]){
	int socketNum = 0;         //socket descriptor
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
    login(argv[1], socketNum);
	clientControl(argv[1], socketNum);
	close(socketNum);
	return 0;
}

void printPrompt(){
	printf("$: ");
	fflush(stdout);
}

void serverClosed(int socket){
	close(socket);
    removeFromPollSet(socket);
	printf("\nServer has terminated\n");
    exit(1);
}

void login(char * handle, int socketNum){
	uint8_t packetLen = strlen(handle) + 2; //add space for flag and length field
	uint8_t flag1 = 1; //flag 1 for login
	uint8_t *packed_handle = packHandle(handle); //get packed handle
	uint8_t handlePlusFlag[packetLen]; //new buffer with space for flag
	memcpy(handlePlusFlag, &flag1, 1); 						//packed handle preceded by flag
	memcpy(handlePlusFlag + 1, packed_handle, packed_handle[0] + 1);
	int sent = sendPDU(socketNum, handlePlusFlag, packetLen);
	//check if sent properly
	if(sent <= 0) serverClosed(socketNum);

	//polling setup
	setupPollSet();
    addToPollSet(socketNum);

	//wait to receive server response
	int readySocket;
	while(readySocket != socketNum) readySocket = pollCall(-1); //poll until a socket is ready
	processMsgFromServer(handle, readySocket);
}

void clientControl(char *handle, int socketNum){
	//add stdin to pollset for client input
    addToPollSet(STDIN_FILENO);
	int readySocket;
	printPrompt();
	while(1){
		readySocket = pollCall(-1); //poll until a socket is ready
		if(readySocket == socketNum) processMsgFromServer(handle, readySocket);
		else if(readySocket == STDIN_FILENO) processStdin(handle, socketNum);
		else{
			close(socketNum);
			perror("poll timeout");
			exit(1);
		}
	}
}

void displayText(uint8_t *packet){
	int offset = 1; //skip flag
	char *srcHandle = unpackHandle(packet + offset);
	uint8_t srcHandleLen = (uint8_t)packet[offset];
	offset += (srcHandleLen + 1); //move past srcHandle
	uint8_t numDest = (uint8_t)packet[offset];
	offset++; //move past num dest 
	for(uint8_t i = 0; i < numDest; i++){
		uint8_t destHandleLen = (uint8_t)packet[offset];
		offset += destHandleLen + 1; //move past destHandle
	}
	// at this point valuePtr should point to the beginning of the text message
	printf("\n%s: %s\n", srcHandle, packet + offset);
	//printPrompt();
}

void displayBroadcast(uint8_t *packet){
	int offset = 1; //skip flag
	char *srcHandle = unpackHandle(packet + offset);
	//uint8_t *valuePtr = packet +1
	uint8_t srcHandleLen = (uint8_t)packet[offset];
	//valuePtr = valuePtr + srcHandleLen + 1; //move valuePtr past srcHandle
	offset += (srcHandleLen + 1); //move past srcHandle
	// at this point valuePtr should point to the beginning of the text message
	printf("\n%s: %s\n", srcHandle, packet + offset);
	printPrompt();
}

void getNumHandles(char * handle, int socketNum, uint8_t *packet){
	removeFromPollSet(STDIN_FILENO);
	uint32_t numNetOrdr, numHostOrdr;
	memcpy(&numNetOrdr, packet + 1, 4);
	numHostOrdr = ntohl(numNetOrdr);
	printf("Number of clients: %d\n", numHostOrdr);
}

void getHandlesList(char * handle, int socketNum, uint8_t *packet){
	char *handleName = unpackHandle(packet + 1);
	printf("   %s\n", handleName);
}

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
			case(5): displayText(dataBuffer); break;
			case(6): displayText(dataBuffer); break;
			case(4): displayBroadcast(dataBuffer); break;
			case(11): getNumHandles(handle, socketNum, dataBuffer); break;
			case(12): getHandlesList(handle, socketNum, dataBuffer); break;
			case(13): {
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

void buildHdr(uint8_t flag, char *clientHandle, uint8_t *buffer){
	uint8_t *clientHandlePacked = packHandle(clientHandle);
	buffer[0] = flag;	// to complete chat header
	memcpy(buffer + 1, clientHandlePacked, strlen(clientHandle) + 1); //packet starts w/ packed src handle
}

int getTxt(uint8_t *buffer, int *inputLen, char *aChar){
	int textLen = 0;
	while ((*inputLen < (MAXPACKET - 1)) && (*aChar != '\n') && (textLen < 199))
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

void sendTxt(int socketNum, uint8_t *buffer, int inputLen){
	buffer[inputLen] = '\0'; // null terminate the string
	inputLen++;
	// for (size_t i = 0; i < inputLen; i++) {
    //     printf("%02X ", buffer[i]);  // Print each byte as a two-digit hex
    // }
    printf("\n");  // Newline after printing all bytes
	sendPDU(socketNum, buffer, inputLen);
}

void messagePacket(uint8_t flag, char *clientHandle, int socketNum){
	uint8_t buffer[MAXPACKET]; //data buffer
	char aChar = 0;
	int inputLen = 0;        
	uint8_t numDest = 0;
	char destHandle[100];
	int sendToSelf = 0;
	
	buildHdr(flag, clientHandle, buffer);
	inputLen = strlen(clientHandle) + 2; // add packed header len and flag to packet len

	if(flag == 5){
		numDest = 1;
	} else{
		if(getchar() != ' '){ //expect space
			printf("Expect space between value\n");
			return; //MAY CAUSE ISSUES
		}
		numDest = getchar() - '0'; //expect num handles
		if((numDest > 9) || (numDest < 2)){
			printf("Invalid number of handles\n");
			return; //MAY CAUSE ISSUES
		}

	}
	buffer[inputLen] = numDest;
	inputLen++;
	if(getchar() != ' '); // expect space
	// get destination handles and add to buffer
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
		memcpy(buffer + inputLen, destHandlePacked, destHandleLen);
		inputLen += destHandleLen;
		//free(destHandlePacked); 		  // copied and no longer needed
	}
	// Read message text
	uint8_t *messageStart = buffer + inputLen;
	//int textLen = getTxt(buffer, &inputLen, &aChar);
	int firstChunkSent = 0;
	while(1) {
		int textLen = getTxt(buffer, &inputLen, &aChar);
	// Loop until the full message is sent
	//while (aChar != '\n' || textLen == 199) {
		// Send current chunk
		sendTxt(socketNum, buffer, inputLen);
		if (!firstChunkSent && sendToSelf) {
			// If sending to self, process the first packet before continuing
			removeFromPollSet(STDIN_FILENO);
			int readySocket = pollCall(-1);
			processMsgFromServer(clientHandle, readySocket);
			addToPollSet(STDIN_FILENO);
			firstChunkSent = 1;
		}
		if ((aChar == '\n') || (textLen < 199)) break;
		inputLen = messageStart - buffer;
	}
}

void broadcastPacket(uint8_t flag, char *clientHandle, int socketNum){
	uint8_t buffer[MAXPACKET]; //data buffer
	int inputLen = 0;
	char aChar = 0;
	buildHdr(flag, clientHandle, buffer);
	inputLen = strlen(clientHandle) + 2; // add packed header len and flag to packet len
	//get text
	int textLen = getTxt(buffer, &inputLen, &aChar);
	//check if message not complete and break it up
	if((aChar != '\n') && (textLen == 199)){
		sendTxt(socketNum, buffer, inputLen);
		//broadcast rest of packet
		broadcastPacket(flag, clientHandle, socketNum);
	} else sendTxt(socketNum, buffer, inputLen);
}

void processStdin(char *handle, int socketNum){
	char command = 0;
	while(command != '%'){
		command = getchar();
	}
	command = getchar();
	switch(tolower(command)){
		case('m'): {
			messagePacket(5, handle, socketNum); 
			printPrompt();
			break;
		}
		case('c'): {
			messagePacket(6, handle, socketNum); 
			printPrompt();
			break;
		}
		case('b'): {
			if(getchar() != ' '){printf("Expect space between value\n");}
			broadcastPacket(4, handle, socketNum);
			printPrompt();
			break;
		}
		case('l'): sendFlag(socketNum, 10); break;
		default: printf("Invalid Command\n"); break;
	}
}

void checkArgs(int argc, char * argv[]){
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle-name host-name port-number \n", argv[0]);
		exit(1);
	}
}
