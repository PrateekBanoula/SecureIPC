#include "ipc_socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: ./secureipc_client STATUS|ANALYZE|SHUTDOWN\n");
        return 1;
    }

    if (strcmp(argv[1], "STATUS") != 0 &&
        strcmp(argv[1], "ANALYZE") != 0 &&
        strcmp(argv[1], "SHUTDOWN") != 0) {
        printf("Invalid command.\n");
        return 1;
    }

    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (client_fd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, IPC_SOCKET_PATH);

    if (connect(
            client_fd,
            (struct sockaddr *)&address,
            sizeof(address)
        ) == -1) {
        perror("Could not connect to manager");
        close(client_fd);
        return 1;
    }

    char request[IPC_REQUEST_SIZE];

    int length = snprintf(
        request,
        sizeof(request),
        "%s\n",
        argv[1]
    );

    if (length < 0 || (size_t)length >= sizeof(request)) {
        fprintf(stderr, "Request is too long.\n");
        close(client_fd);
        return 1;
    }

    if (write(client_fd, request, (size_t)length) != length) {
        perror("Request transmission failed");
        close(client_fd);
        return 1;
    }

    shutdown(client_fd, SHUT_WR);

    printf("========== SERVER RESPONSE ==========\n");

    char response[256];
    ssize_t received;

    while ((received = read(
                client_fd,
                response,
                sizeof(response) - 1
            )) > 0) {
        response[received] = '\0';
        printf("%s", response);
    }

    if (received == -1)
        perror("Response reception failed");

    close(client_fd);
    return received == -1 ? 1 : 0;
}