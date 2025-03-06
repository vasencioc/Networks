// 
// Writen by Victoria Asencio-Clemens, March 2025
//
// PDU creation

#ifndef __PDU_H__
#define __PDU_H__

#define MAX_FILENAME 100
#define HEADER_LEN 7
#define MAX_PAYLOAD 1400
#define MAX_PDU 1407
#define MAX_WINDOW 1073741823
#define RRSREJ_LEN 11
#define FLAG_RR 5
#define FLAG_SREJ 6
#define FLAG_FILE_REQ 8
#define FLAG_FILE_RES 9
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