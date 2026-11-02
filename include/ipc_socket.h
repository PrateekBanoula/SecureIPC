#ifndef IPC_SOCKET_H
#define IPC_SOCKET_H
#include <stddef.h>
#include <sys/types.h>
#include <stddef.h>

#define IPC_SOCKET_PATH "/tmp/secureipc.sock"
#define IPC_REQUEST_SIZE 128
#define IPC_RESPONSE_SIZE 1024

int ipc_socket_server_create(void);
int ipc_socket_accept_client(int server_fd);
int ipc_socket_authenticate_client(int client_fd, uid_t allowed_uid);
int ipc_socket_read_request(int client_fd, char *buffer, size_t size);
int ipc_socket_send_response(int client_fd, const char *response);
void ipc_socket_close_client(int client_fd);
void ipc_socket_close_server(int server_fd);

#endif