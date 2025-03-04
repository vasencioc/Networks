// 
// Written by Victoria Asencio-Clemens, March 2025
// 
// Unit tests for Window Circular Buffer

#include "windowLib.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_create_destroy_window() {
    WindowBuff window = createWindow(10, 20);
    assert(window.windowLen == 10);
    assert(window.valueLen == 20);
    destroyWindow(&window);
    printf("test_create_destroy_window passed.\n");
}

void test_add_get_val() {
    WindowBuff window = createWindow(5, 10);
    uint8_t samplePDU[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    addVal(&window, samplePDU, 17, 1);
    WindowVal retrieved = getVal(&window, 1);
    assert(retrieved.sequenceNum == 1);
    assert(retrieved.dataLen == 10);
    assert(memcmp(retrieved.PDU, samplePDU, 10) == 0);
    destroyWindow(&window);
    printf("test_add_get_val passed.\n");
}

void test_slide_window() {
    WindowBuff window = createWindow(5, 10);
    uint8_t samplePDU[10] = {0};
    for (uint32_t i = 0; i < 5; i++) {
        addVal(&window, samplePDU, 17, i);
    }
    slideWindow(&window, 3);
    assert(window.lower == 4);
    destroyWindow(&window);
    printf("test_slide_window passed.\n");
}

void test_window_check() {
    WindowBuff window = createWindow(5, 10);
    assert(windowCheck(&window) == 0);
    destroyWindow(&window);
    printf("test_window_check passed.\n");
}

int main() {
    test_create_destroy_window();
    test_add_get_val();
    test_slide_window();
    test_window_check();
    printf("All tests passed successfully.\n");
    return 0;
}
