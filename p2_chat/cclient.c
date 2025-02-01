
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>
#include <ctype.h>

#include "pollLib.h"
#include "networks.h"
#include "safeUtil.h"
#include "myPDUfncs.h"
#include "HandleTable.h"

#define MAXBUF 1024
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
int readFromStdin(uint8_t * buffer);
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

void serverClosed(int socket){
	close(socket);
    removeFromPollSet(socket);
	printf("\nServer has terminated\n");
    exit(1);
}

void login(char * handle, int socketNum){
	uint8_t packetLen = strlen(handle) + 2; //add space for flag and length field
	uint8_t flag1 = 1; //flag 1 for login
	uint8_t *packed_handle = packHandle(handle); 			//get packed handle
	uint8_t *handlePlusFlag = (uint8_t *)malloc(packetLen); //new buffer with space for flag
	memcpy(handlePlusFlag, &flag1, 1); 						//packed handle preceded by flag
	memcpy(handlePlusFlag + 1, packed_handle, packed_handle[0] + 1);
	free(packed_handle);									//free packed handle before sending packet
	int sent = sendPDU(socketNum, handlePlusFlag, packetLen);
	free(handlePlusFlag); //no longer needed
	//check if sent properly
	if(sent <= 0){
		serverClosed(socketNum);
	}

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
	while(1){
		printf("$: ");
		fflush(stdout);
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
	//uint8_t *valuePtr = packet +1
	uint8_t srcHandleLen = (uint8_t)packet[offset];
	//valuePtr = valuePtr + srcHandleLen + 1; //move valuePtr past srcHandle
	offset += (srcHandleLen + 1); //move past srcHandle
	uint8_t numDest = (uint8_t)packet[offset];
	offset++; //move past num dest 
	for(uint8_t i = 0; i < numDest; i++){
		uint8_t destHandleLen = (uint8_t)packet[offset];
		offset += destHandleLen + 1; //move past destHandle
	}
	// at this point valuePtr should point to the beginning of the text message
	printf("%s: %s\n", srcHandle, packet + offset);
	free(srcHandle);
}

void displayBroadcast(uint8_t *packet){
	int offset = 1; //skip flag
	char *srcHandle = unpackHandle(packet + offset);
	//uint8_t *valuePtr = packet +1
	uint8_t srcHandleLen = (uint8_t)packet[offset];
	//valuePtr = valuePtr + srcHandleLen + 1; //move valuePtr past srcHandle
	offset += (srcHandleLen + 1); //move past srcHandle
	// at this point valuePtr should point to the beginning of the text message
	printf("%s: %s\n", srcHandle, packet + offset);
	free(srcHandle);
}

void getNumHandles(char * handle, int socketNum, uint8_t *packet){
	uint32_t numNetOrdr, numHostOrdr;
	memcpy(&numNetOrdr, packet + 1, 4);
	numHostOrdr = ntohl(numNetOrdr);
	printf("Number of clients: %d\n", numHostOrdr);
	int readySocket = pollCall(-1); //poll until next handle ready;
	if(readySocket == socketNum) processMsgFromServer(handle, readySocket);
	else{close(socketNum);}
}

void getHandlesList(char * handle, int socketNum, uint8_t *packet){
	char *handleName = unpackHandle(packet + 1);
	printf("\t%s\n", handleName);
	free(handleName);
	int readySocket = pollCall(-1); //poll until next handle ready;
	if(readySocket == socketNum) processMsgFromServer(handle, readySocket);
	else{close(socketNum);}
	//free(handleName);
}

void processMsgFromServer(char *handle, int socketNum){
    uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	//now get the data from the server socket
	if((messageLen = recvPDU(socketNum, dataBuffer, MAXBUF)) > 0){
		//printf("\nMessage received on socket: %d, length: %d Data: %s\n", socketNum, messageLen, dataBuffer + 2);
		uint8_t flag = dataBuffer[0];
		switch(flag) {
			case(FLAG2): printf("Login Successful\n"); break;
			case(FLAG3): printf("Handle Already Exists\n"); exit(-1);
			case(5): displayText(dataBuffer); break;
			case(6): displayText(dataBuffer); break;
			case(4): displayBroadcast(dataBuffer); break;
			case(11): getNumHandles(handle, socketNum, dataBuffer); break;
			case(12): getHandlesList(handle, socketNum, dataBuffer); break;
			case(13): {
				addToPollSet(STDIN_FILENO);
				fflush(stdin);
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

int messagePacket(uint8_t flag, char *clientHandle, uint8_t * buffer, int socketNum){
	//int sent = 0;
	char aChar = 0;
	int inputLen = 0;        
	uint8_t numDest = 0;
	char destHandle[100];
	
	uint8_t *clientHandlePacked = packHandle(clientHandle);
	buffer[0] = flag;	// to complete chat header
	memcpy(buffer + 1, clientHandlePacked, strlen(clientHandle) + 1); //packet starts w/ packed src handle
	free(clientHandlePacked); 		     // copied and no longer needed
	inputLen = strlen(clientHandle) + 2; // add packed header len and flag to packet len

	if(flag == 5){
		numDest = 1;
	} else{
		if(getchar() != ' '){ //expect space
			printf("Expect space between value\n");
			return 0; //MAY CAUSE ISSUES
		}
		numDest = getchar() - '0'; //expect num handles
		if((numDest > 9) || (numDest < 2)){
			printf("Invalid number of handles\n");
			return 0; //MAY CAUSE ISSUES
		}

	}
	buffer[inputLen] = numDest;
	inputLen++;
	aChar = getchar(); // expect space
	// get destination handles and add to buffer
	for(uint8_t i = 0; i < numDest; i++){
		uint8_t destHandleLen = 0;
		aChar = getchar();
		while(aChar != ' '){
			destHandle[destHandleLen] = aChar;
			destHandleLen++;
			aChar = getchar();
		}
		destHandle[destHandleLen] = '\0'; //null terminate handle name
		destHandleLen++;
		uint8_t * destHandlePacked = packHandle(destHandle);
		//add packed dest handle to packet
		memcpy(buffer + inputLen, destHandlePacked, destHandleLen);
		inputLen += destHandleLen;
		free(destHandlePacked); 		  // copied and no longer needed
	}
	int textLen = 0;
	while ((inputLen < (MAXBUF - 1)) && (aChar != '\n') && (textLen < 199))
	{
		aChar = getchar();
		if (aChar != '\n')
		{	
			buffer[inputLen] = aChar;
			inputLen++;
			textLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	//sent = 
	return sendPDU(socketNum, buffer, inputLen);
	//printf("sent: %s len: %d (including null)\n", buffer, sent);
}

int broadcastPacket(uint8_t flag, char *clientHandle, uint8_t * buffer, int socketNum){
	char aChar = 0;
	int inputLen = 0; 
	int textLen = 0;
	uint8_t *clientHandlePacked = packHandle(clientHandle);
	buffer[0] = flag;	// to complete chat header
	memcpy(buffer + 1, clientHandlePacked, strlen(clientHandle) + 1); //packet starts w/ packed src handle
	free(clientHandlePacked); 		     // copied and no longer needed
	inputLen = strlen(clientHandle) + 2; // add packed header len and flag to packet len
	if(getchar() != ' '){printf("Expect space between value\n");}
	while ((inputLen < (MAXBUF - 1)) && (aChar != '\n') && (textLen < 199))
	{
		aChar = getchar();
		if (aChar != '\n')
		{	
			buffer[inputLen] = aChar;
			inputLen++;
			textLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	return sendPDU(socketNum, buffer, inputLen);
}

void processStdin(char *handle, int socketNum){
    uint8_t sendBuf[MAXBUF]; //data buffer
	//int sendLen = 0;         //amount of data to send
	int sent = 0;            //actual amount of data sent

	char command = 0;
	while((command != '%') && (command !='\n')){
		command = getchar();
	}
	if(command == '\n'){
		printf("No Command Entered\n"); 
		return;
	} else{
		char command = getchar();
		switch(tolower(command)){
			case('m'): sent = messagePacket(5, handle, sendBuf, socketNum); break;
			case('c'): sent = messagePacket(6, handle, sendBuf, socketNum); break;
			case('b'): sent = broadcastPacket(4, handle,sendBuf, socketNum); break;
			case('l'): {
				sent = sendFlag(socketNum, 10);
				removeFromPollSet(STDIN_FILENO);
				int readySocket = pollCall(-1); //poll until next handle ready;
				if(readySocket == socketNum) processMsgFromServer(handle, readySocket);
				else{close(socketNum);}
				break;
			}
			default: printf("Invalid Command\n"); break;
		}
	}
	if(sent <= 0) serverClosed(socketNum);
	//get data from stdin
	//sendLen = readFromStdin(sendBuf);
	//printf("sent: %s string len: %d (including null)\n", sendBuf, sendLen);
	// //send data from stdin
	// sent = sendPDU(socketNum, sendBuf, sendLen);
	// if (sent > 0)
	// {
	// 	printf("Amount of data sent is: %d\n", sent);
	// } 
    // //clean up if connection closed
	// else{
    //     close(socketNum);
    //     removeFromPollSet(socketNum);
    //     //remove socket from handle table in P2
	// 	printf("\nServer has terminated\n");
    //     exit(1);
	// }
}

int readFromStdin(uint8_t * buffer){
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

void checkArgs(int argc, char * argv[]){
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle-name host-name port-number \n", argv[0]);
		//printf("usage: %s host-name port-number \n", argv[0]);
		exit(1);
	}
}
