#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 8192
#define CACHE_CAPACITY 10


typedef struct CacheNode {
    char url[512];
    char *response;
    int response_size;
    struct CacheNode *next;
} CacheNode;

