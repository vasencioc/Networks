// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//
// Handle table function prototypes

#ifndef __WINDOWLIB_H__
#define __WINDOWLIB_H__
#define _POSIX_C_SOURCE 200809L  // Required for strdup on some systems
#include <stdint.h>
#include <stdlib.h> 
#include <string.h>

/* WindowVal: Represents one entry in the window buffer*/
typedef struct WindowVal{
    uint32_t dataLen;
    uint32_t sequenceNum;
    uint8_t *PDU;
} WindowVal;

/*Window Buffer as */
typedef struct WindowBuff{
    uint32_t lower;
    uint32_t upper;
    uint32_t current;
    uint32_t windowLen;
    uint32_t valueLen;
    WindowVal *windowBuff;
} WindowBuff;

/* handle table control functions */
WindowBuff createWindow(uint32_t windowLen, uint32_t valueLen);
void destroyWindow(WindowBuff *window);
void addVal(WindowBuff *window, uint8_t *PDU, uint32_t sequenceNum);
WindowVal getVal(WindowBuff *window, uint32_t sequenceNum);
void slideWindow(WindowBuff *window, uint32_t sequenceNum);
int windowCheck(WindowBuff *window);

#endif