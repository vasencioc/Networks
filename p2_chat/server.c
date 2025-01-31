
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

#define FLAG1 1
#define FLAG2 2
#define FLAG3 3

void clientClosed(int socket);
void clientLogin(int clientSocket, HandleTable *table, uint8_t * dataBuffer);
void serverControl(int mainServerSocket);
void addNewSocket(int readySocket);
void processClient(int clientSocket, HandleTable *table);
int checkArgs(int argc, char *argv[]);

int main(int argc, char *argv[]) {
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

void clientLogin(int clientSocket, HandleTable *table, uint8_t * handleBuffer){
	char *handle = unpackHandle(handleBuffer);
	int sent = 0;
	if(getSocket(table, handle) != FAILURE) {
		free(handle);
		sent = sendFlag(clientSocket, FLAG3);
	} else if(addHandle(table, handle, clientSocket) == FAILURE){
		free(handle);
		printf("Can't Overwrite Handle\n");
		sent = sendFlag(clientSocket, FLAG3);
	} else{
		sent = sendFlag(clientSocket, FLAG2);
	}
	if(sent <= 0) clientClosed(clientSocket);
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
			destroyTable(&handleTable);
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

void forwardMessage(HandleTable *table, uint8_t *packet, int packetLen){
	int sent = 0;
	int offset = 1; //skip flag
	uint8_t srcHandleLen = (uint8_t)packet[offset];
	offset += (srcHandleLen + 1); //move past srcHandle
	uint8_t numDest = (uint8_t)packet[offset];
	offset++; //move past num dest 

	for(uint8_t i = 0; i < numDest; i++){
		char *destHandle = unpackHandle(packet + offset);
		uint8_t destHandleLen = (uint8_t)packet[offset];
		offset += destHandleLen + 1; //move past destHandle
		int destSocket = getSocket(table, destHandle);
		if(destSocket != FAILURE){
			sent = sendPDU(destSocket, packet, packetLen);
			if(sent <= 0) {clientClosed(destSocket);}
			else if(sent != (packetLen + 2)) {printf("Error Forwarding text\n");}
		} else {printf("Client with handle %s does not exist.", destHandle);}
		free(destHandle);
	}
}

void broadcastMessage(HandleTable *table, uint8_t *packet, int packetLen){
	int sent = 0;
	int offset = 1; //skip flag
	char* srcHandle = unpackHandle(packet + offset);

	for(int i = 0; i < table->cap; i++){
        if(table->arr[i] && (strcmp(table->arr[i],srcHandle) != 0)){
            sent = sendPDU(i, packet, packetLen);
			if(sent <= 0) {clientClosed(i);}
			else if(sent != (packetLen + 2)) {printf("Error Forwarding text\n");}
        }
    }
	free(srcHandle);
}

void sendHandles(HandleTable * table, int socket){
	int sent = 0;
	uint8_t flag11 = 11;
	uint8_t flag12 = 12;
	uint32_t numHandles = htonl(table->size);
	uint8_t dataBuffer[5];
	memcpy(dataBuffer, &flag11, 1);
	memcpy(dataBuffer + 1, &numHandles, 4);
	sent = sendPDU(socket, dataBuffer, 5);
	if(sent <= 0) {clientClosed(socket);}
	else{
		for(int i = 0; i < table->cap; i++){
			if(table->arr[i]){
				uint8_t *handle = packHandle(table->arr[i]);
				int pktLen = handle[0] + 2;
				uint8_t pkt[pktLen];
				memcpy(pkt, &flag12, 1);
				memcpy(pkt + 1, handle, pktLen - 1);
				sent = sendPDU(socket, pkt, pktLen);
				if(sent <= 0) {clientClosed(socket);}
			}
    	}
	}
}

void processClient(int clientSocket, HandleTable *table){
	uint8_t flag;
	int messageLen = 0;
    uint8_t dataBuffer[MAXBUF];
	//now get the data from the client_socket
    messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF);
	//char *message = messagePacking(dataBuffer);
	//check if connection was closed or error
	if(messageLen > 0){
		printf("Message received on socket: %d, length: %d Data: %s\n", clientSocket, messageLen, dataBuffer + 2);
		flag = dataBuffer[0];
		switch(flag) {
			case(FLAG1): clientLogin(clientSocket, table, dataBuffer + 1); break;
			case(5): forwardMessage(table, dataBuffer, messageLen); break;
			case(6): forwardMessage(table, dataBuffer, messageLen); break;
			case(4): broadcastMessage(table, dataBuffer, messageLen); break;
			case(10): sendHandles(table, clientSocket); break;
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

