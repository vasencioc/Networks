// 
// Writen by Victoria Asencio-Clemens, March 2025
//
// Server setup and control for rcopy

#include "poll.h"
#include "networks.h"
#include "safeUtil.h"
#include "windowLib.h"
#include "PDU.h"

/*** Function Prototypes ***/
void serverControl(int mainServerSocket);
void processClient(int clientSocket, HandleTable *table);
/*** Source Code ***/

int main(int argc, char *argv[]) {
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	portNumber = checkArgs(argc, argv);
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
	//polling set setup
	setupPollSet();
	addToPollSet(mainServerSocket);
	//control loop
	while(1){
		//poll until a socket is ready
		int readySocket = pollCall(-1);
		if(readySocket == mainServerSocket){ 
			addToPollSet(newSocket); //setup client connection
		}else if(readySocket < 0){
			perror("poll timeout");
			exit(1);
		}
		else if (readySocket > mainServerSocket) {processClient(readySocket);}
	}
}

/* Control client communication*/
void processClient(int clientSocket){
	uint8_t flag;
	int messageLen = 0;
    uint8_t dataBuffer[MAXPACKET];
	memset(dataBuffer, 0, MAXPACKET);
	//now get the data from the client_socket
    messageLen = recvPDU(clientSocket, dataBuffer, MAXPACKET);
	//check if connection was closed or error
	if(messageLen > 0){
		flag = dataBuffer[0];
		switch(flag) {
			case(FLAG8): clientLogin(clientSocket, dataBuffer + 1); break;
			default: break;
		}
	}
	//clean up
	else {
		clientClosed(clientSocket);
		if(getHandle(table, clientSocket)) removeHandle(table, clientSocket);
	}
}
