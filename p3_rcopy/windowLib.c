// 
// Writen by Victoria Asencio-Clemens, March 2025
//

#include "windowLib.h"

WindowBuff *createWindow(uint32_t windowLength, uint32_t valueLength){
    uint32_t i;
    WindowBuff *window = malloc(sizeof(WindowBuff));
    window->windowLen = windowLength;
    window->valueLen = valueLength;
    window->lower = 0;
    window->upper = window->windowLen - 1;
    window->current = 0;
    window->buffer = (WindowVal *)malloc(sizeof(WindowVal) * windowLength);
    if (!window->buffer) {
        printf("Buffer allocation failed\n");
        exit(-1);
    }
    for(i = 0; i < window->windowLen; i++){
        window->buffer[i].dataLen = 0;
        window->buffer[i].sequenceNum = 0;
        window->buffer[i].PDU = NULL;
    }
    return window;
}

void destroyWindow(WindowBuff *window){
    uint32_t i;
    for(i = 0; i < window->windowLen; i++){
        if(window->buffer[i].PDU != NULL) free(window->buffer[i].PDU);
    }
    free(window->buffer);
}

void addWinVal(WindowBuff *window, uint8_t *PDU, uint32_t PDUlen, uint32_t sequenceNum){
    if(!window || !window->buffer) return;
    uint32_t index = sequenceNum % window->windowLen;
    if(window->buffer[index].PDU != NULL){
        free(window->buffer[index].PDU);
    }
    window->buffer[index].dataLen = PDUlen - 7;
    window->buffer[index].sequenceNum = sequenceNum;
    window->buffer[index].PDU = (uint8_t *)malloc(sizeof(uint8_t) * PDUlen);
    if (!window->buffer[index].PDU) {
        printf("Buffer value allocation failed\n");
        exit(-1);
    }
    memcpy(window->buffer[index].PDU, PDU, PDUlen);
}

WindowVal getWinVal(WindowBuff *window, uint32_t sequenceNum){
    if(!window || !window->buffer){
        printf("Invalid window buffer");
        exit(-1);
    }
    uint32_t index = sequenceNum % window->windowLen;
    return window->buffer[index];
}

void slideWindow(WindowBuff *window, uint32_t newLower){
    if(!window || !window->buffer) return;
    while(window->lower <= newLower){
        uint32_t index = window->lower % window->windowLen;
        free(window->buffer[index].PDU);
        window->buffer[index].PDU = NULL;
        window->buffer[index].dataLen = 0;
        window->buffer[index].sequenceNum = 0;
        window->lower++;
    }
    window->upper = window->lower + window->windowLen - 1;
}

int windowCheck(WindowBuff *window){
    if(!window || !window->buffer){
        printf("Invalid buffer");
        exit(-1);
    }
    return (window->current >= window->upper);
}
