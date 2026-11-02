#define _GNU_SOURCE

#include "ipc_socket.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>

int ipc_socket_server_create(void)
{
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;

    if (strlen(IPC_SOCKET_PATH) >= sizeof(address.sun_path)) {
        fprintf(stderr, "Socket path is too long.\n");
        close(server_fd);
        return -1;
    }

    strcpy(address.sun_path, IPC_SOCKET_PATH);

    /* Remove only a stale socket, never an unrelated file. */
    struct stat info;

    if (lstat(IPC_SOCKET_PATH, &info) == 0) {
        if (!S_ISSOCK(info.st_mode)) {
            fprintf(stderr, "Socket path exists and is not a socket.\n");
            close(server_fd);
            return -1;
        }

        if (unlink(IPC_SOCKET_PATH) == -1) {
            perror("unlink stale socket");
            close(server_fd);
            return -1;
        }
    } else if (errno != ENOENT) {
        perror("lstat");
        close(server_fd);
        return -1;
    }

    mode_t old_mask = umask(0077);

    int bind_result = bind(
        server_fd,
        (struct sockaddr *)&address,
        sizeof(address)
    );

    umask(old_mask);

    if (bind_result == -1) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (chmod(IPC_SOCKET_PATH, 0600) == -1) {
        perror("chmod socket");
        close(server_fd);
        unlink(IPC_SOCKET_PATH);
        return -1;
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        unlink(IPC_SOCKET_PATH);
        return -1;
    }

    return server_fd;
}

int ipc_socket_accept_client(int server_fd)
{
    int client_fd;

    do {
        client_fd = accept(server_fd, NULL, NULL);
    } while (client_fd == -1 && errno == EINTR);

    if (client_fd == -1)
        perror("accept");

    return client_fd;
}

int ipc_socket_authenticate_client(int client_fd, uid_t allowed_uid)
{
    struct ucred credentials;
    socklen_t length = sizeof(credentials);

    if (getsockopt(
            client_fd,
            SOL_SOCKET,
            SO_PEERCRED,
            &credentials,
            &length
        ) == -1) {
        perror("SO_PEERCRED");
        return -1;
    }

    printf(
        "[Security] Client PID=%d UID=%d GID=%d\n",
        credentials.pid,
        credentials.uid,
        credentials.gid
    );

    if (credentials.uid != allowed_uid) {
        fprintf(stderr, "[Security] Client rejected: UID mismatch.\n");
        return -1;
    }

    printf("[Security] Client authenticated.\n");
    return 0;
}

int ipc_socket_read_request(int client_fd, char *buffer, size_t size)
{
    if (!buffer || size < 2)
        return -1;

    size_t used = 0;

    while (used < size - 1) {
        char ch;
        ssize_t received = read(client_fd, &ch, 1);

        if (received == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (received == 0)
            return -1;

        if (ch == '\n') {
            buffer[used] = '\0';
            return used > 0 ? 0 : -1;
        }

        if (ch == '\0')
            return -1;

        buffer[used++] = ch;
    }

    /* Request exceeded the allowed length. */
    return -1;
}

int ipc_socket_send_response(int client_fd, const char *response)
{
    if (!response)
        return -1;

    size_t length = strlen(response);
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = write(
            client_fd,
            response + sent,
            length - sent
        );

        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (n == 0)
            return -1;

        sent += (size_t)n;
    }

    return 0;
}

void ipc_socket_close_client(int client_fd)
{
    if (client_fd >= 0)
        close(client_fd);
}

void ipc_socket_close_server(int server_fd)
{
    if (server_fd >= 0)
        close(server_fd);

    unlink(IPC_SOCKET_PATH);
}