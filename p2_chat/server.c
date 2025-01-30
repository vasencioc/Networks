
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

void serverControl(int mainServerSocket);
void addNewSocket(int readySocket);
void processClient(int clientSocket);
int checkArgs(int argc, char *argv[]);

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);
    HandleTable handleTable = createtable();
	//manage server communication
	serverControl(mainServerSocket);
	/* close the sockets */
	close(mainServerSocket);
	return 0;
}

void serverControl(int mainServerSocket){
	//polling set setup
	setupPollSet();
	addToPollSet(mainServerSocket);
	while(1){
		//poll until a socket is ready
		int readySocket = pollCall(-1);
		if(readySocket == mainServerSocket){ 
			addNewSocket(readySocket);
		}else if(readySocket < 0){
			perror("poll timeout");
			exit(1);
		}
		else if (readySocket > mainServerSocket) {processClient(readySocket);}
	}
}

void addNewSocket(int readySocket){
	//accept client and add to pollset
    int newSocket = tcpAccept(readySocket, DEBUG_FLAG);
    addToPollSet(newSocket);
}

void processClient(int clientSocket){
	int messageLen = 0;
    uint8_t dataBuffer[MAXBUF];
	//now get the data from the client_socket
    messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF);
	//check if connection was closed or error
	if(messageLen > 0){
		printf("Message received on socket: %d, length: %d Data: %s\n", clientSocket, messageLen, dataBuffer + 2);
		
		//Echo message received for testing purposes
        uint8_t repeatMSG[messageLen];
        memcpy(&repeatMSG, dataBuffer + 2, messageLen);
        int sent = sendPDU(clientSocket, repeatMSG, messageLen);
        if (sent > 0)
        {
            printf("Amount of data sent is: %d\n", sent);
        } 
        else {
			printf("Client on socket %d has terminated\n", clientSocket);
        	close(clientSocket);
        	removeFromPollSet(clientSocket);
			//remove socket from handle table in P2
        }
	}
	//clean up
	else {
		printf("Client on socket %d has terminated\n", clientSocket);
        close(clientSocket);
        removeFromPollSet(clientSocket);
        //remove socket from handle table in P2
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

