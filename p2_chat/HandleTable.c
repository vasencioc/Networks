// 
// Writen by Victoria Asencio-Clemens, Jan. 2025
//

#include "HandleTable.h"

HandleTable createTable(){
    //create table with initial capacity
    HandleTable table;
    table.cap = INITIAL_CAPACITY;
    table.arr = (char **)malloc(sizeof(char *) * table.cap);
    if(table.arr == NULL){
        table.cap = 0;
        return table;
    }
    //inititalize table entries to NULL
    for(int i = 0; i < table.cap; i++){
        table.arr[i] = NULL;
    }
    table.size = 0;
    return table;
}

void destroyTable(HandleTable *table){
    // check if it's a valid table
    if (table == NULL) return;
    //free handles
    for(int i = 0; i < table->cap; i++){
        if(table->arr[i] != NULL){
            free(table->arr[i]);
        }
    }
    // free table
    free(table->arr);
    table->cap = 0;
    table->size = 0;
    return;
}

int addHandle(HandleTable *table, char *handle, int socket){
    if(!table || !handle) return FAILURE;
    //make sure take is correct size
    if(socket >= table->cap){
        if(resizeTable(table, socket + 1) == FAILURE) return FAILURE;
    }
    //make sure handle/socket isn't in table yet
    else if(table->arr[socket]){
        return FAILURE;
    }
    //add handle to handle table
    table->arr[socket] = malloc(strlen(handle) + 1);
    strcpy(table->arr[socket], handle);
    table->size++;
    return SUCCESS;
}

void removeHandle(HandleTable *table, int socket){
    //make sure socket is in the tabke already
    if((socket < table->cap) && table && (table->arr[socket] != NULL)){
        free(table->arr[socket]); //free memory for handle
        table->arr[socket] = NULL; //set to null value
        table->size--;
    }
    return;
}

int resizeTable(HandleTable *table, int newSize){
    if(!table || (newSize <= table->cap)) return FAILURE;
    char **newArr = (char **)realloc(table->arr, sizeof(char *) * newSize);
    if(newArr){
        table->arr = newArr;
        for(int i = table->cap; i < newSize; i++){
            table->arr[i] = NULL;
        }
        table->cap = newSize;
        return SUCCESS;
    }
    
    return FAILURE;
}

int getSocket(HandleTable *table, char *handle){
    if(!table || !handle) return FAILURE;

    for(int i = 0; i < table->cap; i++){
        if(table->arr[i] && (strcmp(table->arr[i],handle) == 0)){
            return i;
        }
    }
    return FAILURE;
}

char * getHandle(HandleTable* table, int socket){
    if(!table || (socket >= table->cap)) return NULL;
    return table->arr[socket];
}
