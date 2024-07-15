#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define PORT 9090
#define SERVER_IP "127.0.0.1"
#define COMMAND_SIZE 1024
char buffer[COMMAND_SIZE];

void error(char* text) {
    perror(text);
    exit(EXIT_FAILURE);
} 

void* read_thread(void* args) {
    int new_fd = *(int*)args;
    int r_bytes = recv(new_fd, buffer, sizeof(buffer), 0);
    while (1) {
        if (r_bytes < 0) {
            error("recv");
        }
        buffer[r_bytes] = '\0';
        printf("Message from server %s\n", buffer);
    }
    return NULL;
}

void* write_thread(void* args) {
    int new_fd = *(int*)args;
    while (1) {
        printf("Enter your command\n");
        fgets(buffer, sizeof(buffer), stdin);
        int w_bytes = send(new_fd, buffer, strlen(buffer), 0);
        if (w_bytes < 0) {
            error("send: ");
        }
        if (strcmp(buffer, "exit") == 0) {
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

int main() {
    struct sockaddr_in server;
    socklen_t server_size = sizeof(server);

   
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_port = htons(PORT);

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        error("socket: ");
    }

    if (connect(sock_fd, (struct sockaddr *)&server, server_size) < 0) {
        error("connect: ");
    }

    pthread_t read_tid, write_tid;

    pthread_create(&read_tid, NULL, read_thread, &sock_fd);
    pthread_create(&write_tid, NULL, write_thread, &sock_fd);

    pthread_join(read_tid, NULL);
    pthread_join(write_tid, NULL);

    close(sock_fd);
}
