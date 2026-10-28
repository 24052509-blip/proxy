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

       if (count >= CACHE_CAPACITY && cache_head != NULL) {
        CacheNode *temp = cache_head;
        cache_head = cache_head->next;
        free(temp->response);
        free(temp);
    }

    CacheNode *newNode = (CacheNode *)malloc(sizeof(CacheNode));
    if (!newNode) {
        LeaveCriticalSection(&cache_lock);
        return;
    }
    strcpy_s(newNode->url, sizeof(newNode->url), url);
    newNode->response = (char *)malloc(size);
    if (!newNode->response) {
        free(newNode);
        LeaveCriticalSection(&cache_lock);
        return;
    }
    memcpy(newNode->response, response, size);
    newNode->response_size = size;
    newNode->next = cache_head;
    cache_head = newNode;

    LeaveCriticalSection(&cache_lock);
}

// Client Handler Thread
DWORD WINAPI handle_client(LPVOID lpParam) {
    client_info *info = (client_info *)lpParam;
    SOCKET client_sock = info->client_socket;
    free(info);

    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        closesocket(client_sock);
        return 0;
    }

        buffer[bytes_received] = '\0';

    // Parse HTTP Request (Method, URL, Host)
    char method[16], url[512], protocol[16];
    if (sscanf(buffer, "%s %s %s", method, url, protocol) < 2) {
        closesocket(client_sock);
        return 0;
    }

    // Extract Host
    char host[256] = "";
    char *host_header = strstr(buffer, "Host: ");
    if (host_header) {
        sscanf(host_header, "Host: %255[^\r\n]", host);
    }

    char log_buf[512];
    sprintf_s(log_buf, sizeof(log_buf), "Request received for URL: %s [Host: %s]", url, host);
    log_message("INFO", log_buf);

    // Access Control check
    if (is_blocked(host) || is_blocked(url)) {
        sprintf_s(log_buf, sizeof(log_buf), "Access Blocked for host: %s", host);
        log_message("BLOCKED", log_buf);
        
        const char *forbidden_response = 
            "HTTP/1.1 403 Forbidden\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<html><body><h1>403 Forbidden</h1><p>Access to this domain is blocked by proxy.</p></body></html>";
        send(client_sock, forbidden_response, (int)strlen(forbidden_response), 0);
        closesocket(client_sock);
        return 0;
    }

      // Check Cache
    CacheNode *cached = find_in_cache(url);
    if (cached != NULL) {
        log_message("CACHE", "Serving response from Cache");
        send(client_sock, cached->response, cached->response_size, 0);
        closesocket(client_sock);
        return 0;
    }

    // Forward request to destination server (Default Port 80)
    struct sockaddr_in server_addr;
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCKET) {
        log_message("ERROR", "Failed to create server socket for forwarding");
        closesocket(client_sock);
        return 0;
    }

    // Resolve Host IP
    struct hostent *he = gethostbyname(host);
    if (he == NULL) {
        log_message("ERROR", "Failed to resolve destination hostname");
        closesocket(server_sock);
        closesocket(client_sock);
        return 0;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    server_addr.sin_addr = *((struct in_addr *)he->h_addr);

    if (connect(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_message("ERROR", "Failed to connect to destination server");
        closesocket(server_sock);
        closesocket(client_sock);
        return 0;
    }

    // Forward original request to destination server
    send(server_sock, buffer, bytes_received, 0);

    // Receive response from server and relay to client + accumulate for cache
    char *full_response = NULL;
    int total_size = 0;
    int resp_bytes;

    while ((resp_bytes = recv(server_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        send(client_sock, buffer, resp_bytes, 0);
        
        // Accumulate for caching
        char *temp = (char *)realloc(full_response, total_size + resp_bytes);
        if (temp) {
            full_response = temp;
            memcpy(full_response + total_size, buffer, resp_bytes);
            total_size += resp_bytes;
        }
    }

    if (full_response && total_size > 0) {
        add_to_cache(url, full_response, total_size);
        free(full_response);
    }

    closesocket(server_sock);
    closesocket(client_sock);
    log_message("INFO", "Request successfully completed and closed.");
    return 0;
}

int main() {
    WSADATA wsaData;
    SOCKET server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(client_addr);

    InitializeCriticalSection(&cache_lock);
    InitializeCriticalSection(&log_lock);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return 1;
    }

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCKET) {
        printf("Socket creation failed.\n");
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed.\n");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    if (listen(server_sock, SOMAXCONN) == SOCKET_ERROR) {
        printf("Listen failed.\n");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    printf("==================================================\n");
    printf(" Windows Multi-Threaded HTTP Proxy Running on Port %d\n", PORT);
    printf("==================================================\n");

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock == INVALID_SOCKET) {
            continue;
        }

        client_info *info = (client_info *)malloc(sizeof(client_info));
        if (!info) {
            closesocket(client_sock);
            continue;
        }
        info->client_socket = client_sock;
        info->client_addr = client_addr;

        DWORD thread_id;
        HANDLE thread = CreateThread(NULL, 0, handle_client, info, 0, &thread_id);
        if (thread != NULL) {
            CloseHandle(thread);
        } else {
            free(info);
            closesocket(client_sock);
        }
    }

    closesocket(server_sock);
    DeleteCriticalSection(&cache_lock);
    DeleteCriticalSection(&log_lock);
    WSACleanup();
    return 0;
}