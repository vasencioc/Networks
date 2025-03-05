// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//
// PDU creation and send and receive functions
#include "PDU.h"

uint8_t *buildPDU(uint8_t* payload, uint32_t payloadLen, uint32_t sequenceNum, uint8_t flag){
    static uint8_t PDU[MAX_PDU];
    uint32_t seqNum_Net = htonl(sequenceNum);
    uint16_t checksum = 0;
    memcpy(PDU, &seqNum_Net, 4);
    memcpy(PDU + 4, &checksum, 2);
    memcpy(PDU + 6, &flag, 1);
    memcpy(PDU + HEADER_LEN, payload, payloadLen);
    checksum = in_cksum((unsigned short *) PDU, payloadLen + HEADER_LEN);
    memcpy(PDU + 4, &checksum, 2);
    return PDU;
}
