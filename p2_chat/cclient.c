
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
void login(char * handle, int socketNum);
void clientControl(int socketNum);
void processMsgFromServer(int socketNum);
void processStdin(int socketNum);
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
    login(argv[1], socketNum);
	clientControl(socketNum);
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
	uint8_t handleLen = strlen(handle) + 1; //add one for null
	int dataLen = FLAG_SIZE + handleLen + 1; //add one for handleLen field
	uint8_t *loginMessage = (uint8_t *)malloc(dataLen); //data buffer
	uint8_t flag1 = FLAG1;
	memcpy(loginMessage, &flag1, FLAG_SIZE); //add flag to PDU
	memcpy(loginMessage + FLAG_SIZE, &handleLen, 1);
	memcpy(loginMessage + FLAG_SIZE + 1, handle, handleLen);

	int sent = sendPDU(socketNum, loginMessage, dataLen);
	if(sent <= 0) serverClosed(socketNum);

	//polling setup
	setupPollSet();
    addToPollSet(socketNum);
	
	//wait to receive server response
	int readySocket;
	while(readySocket != socketNum) readySocket = pollCall(-1); //poll until a socket is ready
	processMsgFromServer(readySocket);
}

void clientControl(int socketNum){
	//add stdin to pollset for client input
    addToPollSet(STDIN_FILENO);
	int readySocket;
	while(1){
		printf("Enter data: ");
		fflush(stdout);
		readySocket = pollCall(-1); //poll until a socket is ready
		if(readySocket == socketNum) processMsgFromServer(readySocket);
		else if(readySocket == STDIN_FILENO) processStdin(socketNum);
		else{
			close(socketNum);
			perror("poll timeout");
			exit(1);
		}
	}
}

void processMsgFromServer(int socketNum){
    uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;

	//now get the data from the server socket
	if((messageLen = recvPDU(socketNum, dataBuffer, MAXBUF)) > 0){
		//printf("\nMessage received on socket: %d, length: %d Data: %s\n", socketNum, messageLen, dataBuffer + 2);
		uint8_t flag = dataBuffer[2];
		switch(flag) {
			case(FLAG2): printf("Login Successful\n"); break;
			case(FLAG3): printf("Handle Already Exists\n"); exit(-1);
			default: break;
		}
	}
	//clean up if connection closed
	else{
        serverClosed(socketNum);
        
	}
}

void processStdin(int socketNum){
    uint8_t sendBuf[MAXBUF]; //data buffer
	int sendLen = 0;         //amount of data to send
	int sent = 0;            //actual amount of data sent
	//get data from stdin
	sendLen = readFromStdin(sendBuf);
	printf("read: %s string len: %d (including null)\n", sendBuf, sendLen);
	//send data from stdin
	sent = sendPDU(socketNum, sendBuf, sendLen);
	if (sent > 0)
	{
		printf("Amount of data sent is: %d\n", sent);
	} 
    //clean up if connection closed
	else{
        close(socketNum);
        removeFromPollSet(socketNum);
        //remove socket from handle table in P2
		printf("\nServer has terminated\n");
        exit(1);
	}
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	//printf("Enter data: ");
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

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle-name host-name port-number \n", argv[0]);
		//printf("usage: %s host-name port-number \n", argv[0]);
		exit(1);
	}
}
