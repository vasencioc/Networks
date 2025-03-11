// 
// Writen by Victoria Asencio-Clemens, March 2025
//
// PDU creation
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <math.h>

#include "pollLib.h"
#include "gethostbyname.h"
#include "checksum.h"
#include "networks.h"
#include "safeUtil.h"
#include "cpe464.h"

#ifndef __PDU_H__
#define __PDU_H__

#define MAX_FILENAME 100
#define HEADER_LEN 7
#define MAX_PAYLOAD 1400
#define MAX_PDU 1407
#define MAX_WINDOW (pow(2, 30) - 1)
#define RRSREJ_LEN 11
#define FLAG_RR 5
#define FLAG_SREJ 6
#define FLAG_FILE_REQ 8
#define FLAG_FILE_RES 9
#define FLAG_FILE_RES_ACK 34
#define FLAG_EOF 10
#define FLAG_EOF_ACK 33
#define FLAG_DATA 16
#define FLAG_SREJ_RES 17
#define FLAG_TIMEOUT_RES 18

struct pdu{
    uint32_t sequenceNum;
    uint16_t checksum;
    uint8_t flag;
    uint8_t payload[MAX_PAYLOAD];
}__attribute__((packed));

uint8_t *buildPDU(uint8_t* payload, uint32_t payloadLen, uint32_t sequenceNum, uint8_t flag);

#endif