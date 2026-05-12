#include <stdio.h>
#include <sys/select.h>
#include <string.h>
#include "server.h"

#define SERVER_PORT 8080

int main() {
    int server_status = start_server(SERVER_PORT);
    if (server_status != 0) {
        return 1;
    }

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_sockfd, &read_fds);
        
        struct timeval timeout = {1, 0};  // 1 second timeout
        int ready = select(server_sockfd + 1, &read_fds, NULL, NULL, &timeout);

        if (ready < 0) {
            perror("select");
            break;
        }
        
        if (ready > 0) {
            accept_client();
        }
    }
    
    stop_server();
    return 0;
}