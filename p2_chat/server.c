// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//
// Server setup and control for chat

#include "pollLib.h"
#include "networks.h"
#include "safeUtil.h"
#include "chatHelpers.h"
#include "HandleTable.h"

/*** Function Prototypes ***/
void clientClosed(int socket);
void clientLogin(int clientSocket, HandleTable *table, uint8_t * dataBuffer);
void serverControl(int mainServerSocket);
void addNewSocket(int readySocket);
void processClient(int clientSocket, HandleTable *table);
int checkArgs(int argc, char *argv[]);
void forwardMessage(int clientSocket, HandleTable *table, uint8_t *packet, int packetLen);
void broadcastMessage(HandleTable *table, uint8_t *packet, int packetLen);
void sendHandles(HandleTable * table, int socket);

/*** Source Code ***/

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

/*Clean-up when client closed*/
void clientClosed(int socket){
	printf("Client on socket %d has terminated\n", socket);
    close(socket);
    removeFromPollSet(socket);
}

/* Log in client and add handle to table*/
void clientLogin(int clientSocket, HandleTable *table, uint8_t * handleBuffer){
	char *handle = unpackHandle(handleBuffer);
	int sent = 0;
	if(getSocket(table, handle) != FAILURE) sent = sendFlag(clientSocket, FLAG3);
	else if(addHandle(table, handle, clientSocket) == FAILURE) sent = sendFlag(clientSocket, FLAG3);
	else sent = sendFlag(clientSocket, FLAG2);
	if(sent <= 0) clientClosed(clientSocket);
}

/*Main control for polling sockets*/
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

/*Set up client connection*/
void addNewSocket(int readySocket){
	//accept client and add to pollset
    int newSocket = tcpAccept(readySocket, DEBUG_FLAG);
    addToPollSet(newSocket);
}

/*Forward message packets ot clients*/
void forwardMessage(int clientSocket, HandleTable *table, uint8_t *packet, int packetLen){
	int sent = 0;
	int offset = 1; //skip flag
	uint8_t srcHandleLen = (uint8_t)packet[offset];
	offset += (srcHandleLen + 1); //move past srcHandle
	uint8_t numDest = (uint8_t)packet[offset];
	offset++; //move past num dest 

	for(uint8_t i = 0; i < numDest; i++){
		char *destHandle = unpackHandle(packet + offset);
		uint8_t destHandleLen = (uint8_t)packet[offset];
		int destSocket = getSocket(table, destHandle);
		if(destSocket != FAILURE){
			sent = sendPDU(destSocket, packet, packetLen);
			if(sent <= 0) clientClosed(destSocket);
		} else { //send error packet if destination client doesn't exist
			uint8_t errBuff[(destHandleLen + 2)]; //space for len field and flag
			uint8_t flagVal = FLAG7;
			memcpy(errBuff, &flagVal, 1);
			memcpy(errBuff + 1, packet + offset, destHandleLen + 1);
			//printf("Client with handle %s does not exist.", destHandle);
			sent = sendPDU(clientSocket, errBuff, destHandleLen + 2);
			if(sent <= 0) clientClosed(destSocket);
		}
		offset += destHandleLen + 1; //move past destHandle
	}
}

/*Forward broadcast messages to all clients*/
void broadcastMessage(HandleTable *table, uint8_t *packet, int packetLen){
	int sent = 0;
	int offset = 1; //skip flag
	char* srcHandle = unpackHandle(packet + offset);

	for(int i = 0; i < table->cap; i++){
        if(table->arr[i] && (strcmp(table->arr[i],srcHandle) != 0)){
            sent = sendPDU(i, packet, packetLen);
			if(sent <= 0) clientClosed(i);
        }
    }
}

/* %L processing: send list of handles*/
void sendHandles(HandleTable * table, int socket){
	int sent = 0;
	uint8_t flag11 = FLAG11;
	uint8_t flag12 = FLAG12;
	uint8_t flag13 = FLAG13;
	//send num handles
	uint32_t numHandles = htonl(table->size);
	uint8_t dataBuffer[5]; //to accomodate chat header + 4 byte num
	memcpy(dataBuffer, &flag11, 1);
	memcpy(dataBuffer + 1, &numHandles, 4);
	sent = sendPDU(socket, dataBuffer, 5);
	if(sent <= 0) {clientClosed(socket);}
	//send handles
	else{
		for(int i = 0; i < table->cap; i++){
			if(table->arr[i]){
				uint8_t *handle = packHandle(table->arr[i]);
				int pktLen = handle[0] + 2;
				uint8_t pkt[pktLen];
				memcpy(pkt, &flag12, 1);
				memcpy(pkt + 1, handle, pktLen - 1);
				sent = sendPDU(socket, pkt, pktLen);
				if(sent <= 0) clientClosed(socket);
			}
    	}
		//send flag 13 to indicate done sending handles
		sendFlag(socket, flag13);
	}
}

/* Control client communication*/
void processClient(int clientSocket, HandleTable *table){
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
			case(FLAG1): clientLogin(clientSocket, table, dataBuffer + 1); break;
			case(FLAG4): broadcastMessage(table, dataBuffer, messageLen); break;
			case(FLAG5): forwardMessage(clientSocket, table, dataBuffer, messageLen); break;
			case(FLAG6): forwardMessage(clientSocket, table, dataBuffer, messageLen); break;
			case(FLAG10): sendHandles(table, clientSocket); break;
			default: break;
		}
	}
	//clean up
	else {
		clientClosed(clientSocket);
		if(getHandle(table, clientSocket)) removeHandle(table, clientSocket);
	}
}

/*Check command line arguments for proper login*/
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

	