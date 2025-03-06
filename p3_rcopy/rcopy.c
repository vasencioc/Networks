// 
// Writen by Victoria Asencio-Clemens, March 2025
//
// Client setup and control for rcopy

#include "poll.h"
#include "networks.h"
#include "safeUtil.h"
#include "bufferLib.h"
#include "PDU.h"

/*** States for FSM ***/
typedef enum {
	SETUP,
	USE,
	TEARDOWN,
	END
} STATE;

/*** Source Code ***/
int main(int argc, char * argv[]){
	int socketNum = 0;         //socket descriptor
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
    struct sockaddr_in6 serverAddress;
    char hostName;
    int serverPort;
	int socketNum = setupUdpClientToServer(serverAddress, hostName, argv[7]);
    login(argv[1], socketNum);
	clientControl(argv[1], socketNum);
	close(socketNum);
	return 0;
}

/* Logs client into server if the connection is valid and the handle is unique */
void login(int socketNum){
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