#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>

#define MAX_CLIENTS 1024
#define BUFFER_SIZE 65536

typedef struct {
    int port;
    char document_root[256];
    char cgi_root[256];
} ServerConfig;

typedef enum {
    STATE_DISCONNECTED = 0,
    STATE_READING,
    STATE_WRITING,
} ConnectionState;

typedef struct {
    int fd;
    ConnectionState state;
    char read_buffer[BUFFER_SIZE];
    int read_pos;
    char *write_buffer;
    int write_pos;
    int write_len;
    int file_fd;
} ClientContext;

// Initializes the listening socket
int server_init(int port);

// Starts the event loop
void server_run(int server_fd, const ServerConfig *config);

// Load server configuration from a file
int load_config(const char *filename, ServerConfig *config);

#endif // SERVER_H
