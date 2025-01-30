
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

void clientClosed(int socket);
void sendFlag(int socket, uint8_t flag);
void clientLogin(int clientSocket, HandleTable *table, uint8_t * dataBuffer);
void serverControl(int mainServerSocket);
void addNewSocket(int readySocket);
void processClient(int clientSocket, HandleTable *table);
int checkArgs(int argc, char *argv[]);

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);
	//manage server communication
	serverControl(mainServerSocket);
	/* close the sockets */
	close(mainServerSocket);
	return 0;
}

void clientClosed(int socket){
	printf("Client on socket %d has terminated\n", socket);
    close(socket);
    removeFromPollSet(socket);
}

void sendFlag(int socket, uint8_t flag){
	uint8_t response[FLAG_SIZE];
	response[0] = flag;
	int sent = sendPDU(socket, response, FLAG_SIZE);
	if(sent <= 0) clientClosed(socket);
}

void clientLogin(int clientSocket, HandleTable *table, uint8_t * dataBuffer){
    uint8_t handleLen = dataBuffer[0];
	uint8_t *handle = (uint8_t *)malloc(handleLen);
	memcpy(handle, dataBuffer + 1, handleLen);
	if(getSocket(table, handle) != FAILURE) {
		free(handle);
		sendFlag(clientSocket, FLAG3);
	} else if(addHandle(table, handle, clientSocket) == FAILURE){
		free(handle);
		printf("Can't Overwrite Handle\n");
		sendFlag(clientSocket, FLAG3);
	} else sendFlag(clientSocket, FLAG2);
}

void serverControl(int mainServerSocket){
	//create handle table
	HandleTable handleTable = createTable();
	if(!handleTable.arr){
		printf("Error creating handle table\n");
		exit(-1);
	}
	//polling set setup
	setupPollSet();
	addToPollSet(mainServerSocket);
	//control loop
	while(1){
		//poll until a socket is ready
		int readySocket = pollCall(-1);
		if(readySocket == mainServerSocket){ 
			addNewSocket(readySocket);
		}else if(readySocket < 0){
			perror("poll timeout");
			exit(1);
		}
		else if (readySocket > mainServerSocket) {processClient(readySocket, &handleTable);}
	}
}

void addNewSocket(int readySocket){
	//accept client and add to pollset
    int newSocket = tcpAccept(readySocket, DEBUG_FLAG);
	//if(clientLogin(newSocket) == FAILURE)
    addToPollSet(newSocket);
}

void processClient(int clientSocket, HandleTable *table){
	int messageLen = 0;
    uint8_t dataBuffer[MAXBUF];
	//now get the data from the client_socket
    messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF);
	//char *message = messagePacking(dataBuffer);
	//check if connection was closed or error
	if(messageLen > 0){
		printf("Message received on socket: %d, length: %d Data: %s\n", clientSocket, messageLen, message);
		uint8_t flag = dataBuffer[0];
		switch(flag) {
			case(FLAG1): clientLogin(clientSocket, table, dataBuffer + 1); break;
			default: break;
		}
	}
	//clean up
	else {
		clientClosed(clientSocket);
		if(getHandle(table, clientSocket)) removeHandle(table, clientSocket);
	}
}


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

