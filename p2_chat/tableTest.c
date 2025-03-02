#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "HandleTable.h"

#define SUCCESS 0
#define FAILURE -1

int main() {
    printf("Starting HandleTable tests...\n\n");
    
    // Create Handle Table
    HandleTable table = createTable();
    if (table.arr == NULL) {
        printf("Error: Failed to allocate memory for HandleTable.\n");
        return 1;
    }
    printf("HandleTable created with initial capacity %d.\n", table.cap);
    
    // Add handles
    char *handle1 = strdup("User1");
    char *handle2 = strdup("User2");
    char *handle3 = strdup("User3");
    
    if (addHandle(&table, handle1, 2) == SUCCESS)
        printf("Added handle 'User1' at socket 2.\n");
    else
        printf("Error: Failed to add handle 'User1'.\n");
    
    if (addHandle(&table, handle2, 5) == SUCCESS)
        printf("Added handle 'User2' at socket 5.\n");
    else
        printf("Error: Failed to add handle 'User2'.\n");
    
    if (addHandle(&table, handle3, 5) == FAILURE) // Should fail (duplicate socket)
        printf("Correctly prevented adding duplicate socket 5.\n");
    else
        printf("Error: Allowed duplicate socket 5.\n");
    
    // Retrieve handle
    char *retrievedHandle = getHandle(&table, 2);
    if (retrievedHandle)
        printf("Retrieved handle at socket 2: %s\n", retrievedHandle);
    else
        printf("Error: Failed to retrieve handle at socket 2.\n");
    
    // Retrieve socket
    int socket = getSocket(&table, handle1);
    if (socket != FAILURE)
        printf("Handle 'User1' found at socket %d.\n", socket);
    else
        printf("Error: Failed to find socket for 'User1'.\n");
    
    // Remove handle
    removeHandle(&table, 2);
    if (getHandle(&table, 2) == NULL)
        printf("Successfully removed handle at socket 2.\n");
    else
        printf("Error: Handle at socket 2 was not removed properly.\n");
    
    // Resize table
    if (resizeTable(&table, 20) == SUCCESS)
        printf("Successfully resized table to capacity 20.\n");
    else
        printf("Error: Failed to resize table.\n");
    
    // Add after resizing
    char *handle4 = strdup("User4");
    if (addHandle(&table, handle4, 15) == SUCCESS)
        printf("Added handle 'User4' at socket 15 after resizing.\n");
    else
        printf("Error: Failed to add handle 'User4' after resizing.\n");
    
    // Destroy table
    destroyTable(&table);
    if (table.cap == 0 && table.size == 0)
        printf("Successfully destroyed HandleTable.\n");
    else
        printf("Error: HandleTable destruction failed.\n");
    
    printf("\nAll tests completed.\n");
    return 0;
}

// #include <stdio.h>
// #include <stdlib.h>
// #include <stdint.h>
// #include <string.h>
// #include "HandleTable.h"

// void printTable(HandleTable *table) {
//     printf("\nCurrent HandleTable (size: %d, cap: %d):\n", table->size, table->cap);
//     for (int i = 0; i < table->cap; i++) {
//         if (table->arr[i]) {
//             printf("Socket %d: %s\n", i, table->arr[i]); // Assumes valid string data
//         } else {
//             printf("Socket %d: [EMPTY]\n", i);
//         }
//     }
//     printf("\n");
// }

// int main() {
//     printf("=== HandleTable Full Functionality Test ===\n");

//     // Step 1: Create HandleTable
//     HandleTable table = createTable();
//     if (table.cap == 0) {
//         printf("Error: Failed to create HandleTable.\n");
//         return FAILURE;
//     }
//     printf("HandleTable created successfully with initial capacity: %d\n", table.cap);
//     printTable(&table);

//     // Step 2: Dynamically allocate handles
//     const char *name1 = "User1";
//     const char *name2 = "UserTwoLong";

//     uint8_t *handle1 = (uint8_t *)malloc(strlen(name1) + 1);  // +1 for null terminator
//     uint8_t *handle2 = (uint8_t *)malloc(strlen(name2) + 1);  

//     if (!handle1 || !handle2) {
//         printf("Error: Memory allocation failed for handles.\n");
//         return FAILURE;
//     }

//     // Step 3: Copy data using memcpy()
//     memcpy(handle1, name1, strlen(name1) + 1);  // Copy exact bytes including '\0'
//     memcpy(handle2, name2, strlen(name2) + 1);

//     // Step 4: Add handles to the table
//     if (addHandle(&table, handle1, 0) == SUCCESS)
//         printf("Successfully added handle1 at socket 0\n");

//     if (addHandle(&table, handle2, 1) == SUCCESS)
//         printf("Successfully added handle2 at socket 1\n");

//     printTable(&table);

//     // Step 5: Test getSocket() and getHandle()
//     printf("Socket for handle1: %d\n", getSocket(&table, handle1));
//     printf("Handle at socket 1: %s\n", getHandle(&table, 1));

//     // Step 6: Remove a handle and verify removal
//     printf("\nRemoving handle at socket 1...\n");
//     removeHandle(&table, 1);
//     printTable(&table);

//     // Step 7: Cleanup
//     destroyTable(&table);
//     printf("HandleTable destroyed successfully.\n");

//     return 0;
// }
