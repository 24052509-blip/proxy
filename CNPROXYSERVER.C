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
CacheNode *cache_head = NULL;
CRITICAL_SECTION cache_lock;
CRITICAL_SECTION log_lock;


typedef struct {
    SOCKET client_socket;
    struct sockaddr_in client_addr;
} client_info;


void log_message(const char *level, const char *message) {
    EnterCriticalSection(&log_lock);
    printf("[%s] %s\n", level, message);
    FILE *f = fopen("proxy_log.txt", "a");
    if (f) {
        fprintf(f, "[%s] %s\n", level, message);
        fclose(f);
    }
    LeaveCriticalSection(&log_lock);
}

    int is_blocked(const char *host) {
    const char *blocked_list[] = {"example.blocked.com", "malicious.com", "ads.com", NULL};
    for (int i = 0; blocked_list[i] != NULL; i++) {
        if (strstr(host, blocked_list[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}


CacheNode* find_in_cache(const char *url) {
    EnterCriticalSection(&cache_lock);
    CacheNode *curr = cache_head;
    while (curr != NULL) {
        if (strcmp(curr->url, url) == 0) {
            LeaveCriticalSection(&cache_lock);
            return curr;
        }
        curr = curr->next;
    }
    LeaveCriticalSection(&cache_lock);
    return NULL;
}


void add_to_cache(const char *url, const char *response, int size) {
    EnterCriticalSection(&cache_lock);
    
    // Simple replacement if cache exceeds limit
    CacheNode *curr = cache_head;
    int count = 0;
    while(curr != NULL) {
        count++;
        curr = curr->next;
    }
