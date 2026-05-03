#include <stdio.h>
#include <signal.h>
#include <sys/select.h>
#include "server.h"

#define SERVER_PORT 8080

volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) {
    keep_running = 0;
}

int main() {
    signal(SIGINT, handle_signal);  // Handle Ctrl+C
    
    int server_status = start_server(SERVER_PORT);
    if (server_status != 0) {
        return 1;
    }

    while (keep_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_sockfd, &read_fds);
        
        struct timeval timeout = {1, 0};  // 1 second timeout
        int ready = select(server_sockfd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (ready > 0) {
            accept_client();
        }
    }
    
    stop_server();
    return 0;
}