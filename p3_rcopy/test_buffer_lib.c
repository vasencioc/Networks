// 
// Written by Victoria Asencio-Clemens, March 2025
// 
// Unit tests for Buffer Circular Library

#include "bufferLib.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_create_destroy_buffer() {
    CircularBuff buffer = createBuffer(10, 20);
    assert(buffer.bufferLen == 10);
    assert(buffer.valueLen == 20);
    destroyBuffer(&buffer);
    printf("test_create_destroy_buffer passed.\n");
}

void test_add_get_val_buffer() {
    CircularBuff buffer = createBuffer(5, 10);
    uint8_t samplePDU[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    addVal(&buffer, samplePDU, 17, 1);
    BufferVal retrieved = getVal(&buffer, 1);
    assert(retrieved.sequenceNum == 1);
    assert(retrieved.dataLen == 10);
    assert(memcmp(retrieved.PDU, samplePDU, 10) == 0);
    destroyBuffer(&buffer);
    printf("test_add_get_val_buffer passed.\n");
}

void test_validity_flags() {
    CircularBuff buffer = createBuffer(5, 10);
    setValid(&buffer, 2);
    assert(validityCheck(&buffer, 2) == 1);
    setInvalid(&buffer, 2);
    assert(validityCheck(&buffer, 2) == 0);
    destroyBuffer(&buffer);
    printf("test_validity_flags passed.\n");
}

int main() {
    test_create_destroy_buffer();
    test_add_get_val_buffer();
    test_validity_flags();
    printf("All buffer tests passed successfully.\n");
    return 0;
}
