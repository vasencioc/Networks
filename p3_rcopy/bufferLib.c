// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//

#include "bufferLib.h"

/* handle table control functions */
CircularBuff createBuffer(uint32_t bufferLen, uint32_t valueLen){
    CircularBuff circularBuff;
    circularBuff.bufferLen = bufferLen;
    circularBuff.valueLen = valueLength;
    circularBuff.buffer = (BufferVal *)malloc(sizeof(BufferVal) * bufferLen);
    if (!circularBuff.buffer) {
        printf("Buffer allocation failed\n");
        exit(-1);
    }
    for(int i = 0; i < window.windowLen; i++){
        circularBuff.buffer[i].dataLen = 0;
        circularBuff.buffer[i].sequenceNum = 0;
        circularBuff.buffer[i].validFlag = 0;
        circularBuff.buffer[i].PDU = NULL;
    }
    return circularBuff;
}

void destroyBuffer(CircularBuff *buffer){
    for(int i = 0; i < buffer->bufferLen; i++){
        if(buffer->buffer[i].PDU != NULL){
            free(buffer->buffer[i].PDU);
    }
    free(window->buffer);
}

void addVal(CircularBuff *buffer, uint8_t *PDU, uint32_t PDUlen, uint32_t sequenceNum){
    if(!buffer || !buffer->buffer) return;
    uint32_t index = sequenceNum % buffer->bufferLen;
    if(buffer->buffer[index].PDU != NULL){
        free(buffer->buffer[index]);
    }
    buffer->buffer[index].dataLen = PDUlen - 7;
    buffer->buffer[index].sequenceNum = sequenceNum;
    buffer->buffer[index].PDU = (uint8_t *)malloc(sizeof(uint8_t) * PDUlen);
    if (!buffer.buffer) {
        printf("Buffer value allocation failed\n");
        exit(-1);
    }
    memcpy(buffer->buffer[index].PDU, PDU, PDUlen);
}

BufferVal getVal(CircularBuff *buffer, uint32_t sequenceNum){
    if(!buffer || !buffer->buffer){
        printf("Invalid buffer")
        exit(-1);
    }
    uint32_t index = sequenceNum % buffer->bufferLen;
    return buffer->buffer[index];
}

void setValid(CircularBuff *buffer, uint32_t sequenceNum){
    if(!buffer || !buffer->buffer){
        return;
    }
    uint32_t index = sequenceNum % buffer->bufferLen;
    buffer->buffer[index].validFlag = 1;
}

void setInvalid(CircularBuff *buffer, uint32_t sequenceNum){
    if(!buffer || !buffer->buffer){
        return;
    }
    uint32_t index = sequenceNum % buffer->bufferLen;
    buffer->buffer[index].validFlag = 0;
}

uint8_t validityCheck(CircularBuff *buffer, uint32_t sequenceNum){
    if(!buffer || !buffer->buffer){
        return;
    }
    uint32_t index = sequenceNum % buffer->bufferLen;
    return buffer->buffer[index].validFlag;
}
