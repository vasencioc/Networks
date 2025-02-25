// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//
// Handle table function prototypes

#ifndef __HANDLETABLE_H__
#define __HANDLETABLE_H__
#define _POSIX_C_SOURCE 200809L  // Required for strdup on some systems
#include <stdint.h>
#include <stdlib.h> 
#include <string.h>
//#define MAXHANDLE 100
#define INITIAL_CAPACITY 10
#define SUCCESS 0
#define FAILURE -1

/* HandleTable: Represents a collection handles mapped to sockets*/
typedef struct HandleTable{
    int size;
    int cap;
    char **arr;
} HandleTable;

/* handle table control functions */
HandleTable createTable();
void destroyTable(HandleTable *table);
int addHandle(HandleTable *table, char *handle, int socket);
void removeHandle(HandleTable *table, int socket);
int resizeTable(HandleTable *table, int newSize);
int getSocket(HandleTable *table, char *handle);
char *getHandle(HandleTable *table, int socket);

#endif