// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//
// Handle table function prototypes

#ifndef __BUFFERLIB_H__
#define __BUFFERLIB_H__
#define _POSIX_C_SOURCE 200809L  // Required for strdup on some systems
#include <stdint.h>
#include <stdlib.h> 
#include <string.h>

/* BufferVal: Represents one entry in the circular queue buffer*/
typedef struct BufferVal{
    uint32_t dataLen;
    uint32_t sequenceNum;
    uint8_t validFlag;
    uint8_t *PDU;
} BufferVal;

/* Circular queue buffer struct */
typedef struct CircularBuff{
    uint32_t bufferLen;
    uint32_t valueLen;
    CircularBuff *buffer;
} CircularBuff;

/* handle table control functions */
CicularBuff createBuffer(uint32_t bufferLen, uint32_t valueLen);
void destroyBuffer(CircularBuff *buffer);
void addVal(CircularBuff *buffer, uint8_t *PDU, uint32_t sequenceNum);
WindowVal getVal(CircularBuff *buffer, uint32_t sequenceNum);
void setValid(CircularBuff *buffer, uint32_t sequenceNum);
void setInvalid(CircularBuff *buffer, uint32_t sequenceNum);
uint8_t validityCheck(CircularBuff *buffer, uint32_t sequenceNum);

#endif