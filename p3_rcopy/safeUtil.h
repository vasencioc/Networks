// 
// Writen by Hugh Smith, Jan. 2023
//
// Put in system calls with error checking.

#ifndef __SAFEUTIL_H__
#define __SAFEUTIL_H__

#include <stdint.h>

int safeRecvFrom(int socket_num, char* data, int data_len, int flags, struct sockaddr *from_address, socklen_t *from_len);
int safeSendTo(int socket_num, char* data, int data_len, int flags, struct sockaddr *to_address, int to_len);

void * srealloc(void *ptr, size_t size);
void * sCalloc(size_t nmemb, size_t size);


#endif