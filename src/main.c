#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int main() {
    // Ignore SIGPIPE to prevent server crash on client disconnect during write
    signal(SIGPIPE, SIG_IGN);

    ServerConfig config;
    load_config("server.conf", &config);

    printf("Starting server on port %d...\n", config.port);
    printf("Document Root: %s\n", config.document_root);
    printf("CGI Root: %s\n", config.cgi_root);

    int listen_fd = server_init(config.port);
    if (listen_fd < 0) {
        return EXIT_FAILURE;
    }

    server_run(listen_fd, &config);

    return EXIT_SUCCESS;
}
