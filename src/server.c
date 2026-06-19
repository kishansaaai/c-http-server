#define _POSIX_C_SOURCE 200809L
#include "server.h"
#include "http.h"
#include "handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
    }
}

int server_init(int port) {
    int listen_fd;
    struct sockaddr_in server_addr;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    set_nonblocking(listen_fd);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, SOMAXCONN) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);
    return listen_fd;
}

int load_config(const char *filename, ServerConfig *config) {
    // Set defaults
    config->port = 8080;
    strncpy(config->document_root, "www", sizeof(config->document_root) - 1);
    strncpy(config->cgi_root, "cgi-bin", sizeof(config->cgi_root) - 1);

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Config file %s not found, using defaults.\n", filename);
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *key = line;
            char *val = eq + 1;
            
            // Trim newline
            char *nl = strchr(val, '\n');
            if (nl) *nl = '\0';
            nl = strchr(val, '\r');
            if (nl) *nl = '\0';

            if (strcmp(key, "port") == 0) {
                config->port = atoi(val);
            } else if (strcmp(key, "document_root") == 0) {
                strncpy(config->document_root, val, sizeof(config->document_root) - 1);
            } else if (strcmp(key, "cgi_root") == 0) {
                strncpy(config->cgi_root, val, sizeof(config->cgi_root) - 1);
            }
        }
    }
    fclose(fp);
    return 0;
}

void server_run(int listen_fd, const ServerConfig *config) {
    ClientContext *clients = calloc(MAX_CLIENTS, sizeof(ClientContext));
    if (!clients) {
        perror("calloc clients");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].state = STATE_DISCONNECTED;
        clients[i].write_buffer = NULL;
    }

    fd_set read_fds, write_fds;

    while (1) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        
        int max_fd = listen_fd;
        FD_SET(listen_fd, &read_fds);

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].state != STATE_DISCONNECTED) {
                if (clients[i].state == STATE_READING) {
                    FD_SET(clients[i].fd, &read_fds);
                } else if (clients[i].state == STATE_WRITING) {
                    FD_SET(clients[i].fd, &write_fds);
                }
                if (clients[i].fd > max_fd) {
                    max_fd = clients[i].fd;
                }
            }
        }

        int activity = select(max_fd + 1, &read_fds, &write_fds, NULL, NULL);
        if (activity < 0) {
            perror("select");
            continue;
        }

        // 1. Accept new connections
        if (FD_ISSET(listen_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
            if (new_fd >= 0) {
                set_nonblocking(new_fd);
                // Find a free slot
                int added = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].state == STATE_DISCONNECTED) {
                        clients[i].fd = new_fd;
                        clients[i].state = STATE_READING;
                        clients[i].read_pos = 0;
                        clients[i].write_buffer = NULL;
                        clients[i].write_pos = 0;
                        clients[i].write_len = 0;
                        clients[i].file_fd = -1;
                        printf("New connection accepted (fd=%d)\n", new_fd);
                        added = 1;
                        break;
                    }
                }
                if (!added) {
                    printf("Too many clients, rejecting fd=%d\n", new_fd);
                    close(new_fd);
                }
            }
        }

        // 2. Handle client I/O
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].state == STATE_DISCONNECTED) continue;
            
            int fd = clients[i].fd;

            if (clients[i].state == STATE_READING && FD_ISSET(fd, &read_fds)) {
                int bytes_read = read(fd, clients[i].read_buffer + clients[i].read_pos, BUFFER_SIZE - 1 - clients[i].read_pos);
                if (bytes_read > 0) {
                    clients[i].read_pos += bytes_read;
                    clients[i].read_buffer[clients[i].read_pos] = '\0';

                    // Check if we reached end of headers
                    if (strstr(clients[i].read_buffer, "\r\n\r\n")) {
                        HttpRequest req;
                        HttpResponse res;
                        http_init_response(&res);

                        if (http_parse_request(clients[i].read_buffer, &req) == 0) {
                            if (strncmp(req.path, "/cgi-bin/", 9) == 0) {
                                handle_cgi(&req, &res, config->cgi_root);
                            } else if (strncmp(req.path, "/uploads/", 9) == 0) {
                                // Serve files from the uploads directory
                                // We can use handle_static_file, but passing "" as document_root because req.path already starts with /uploads/
                                handle_static_file(&req, &res, ".");
                            } else if (strcmp(req.path, "/upload") == 0) {
                                handle_upload(&req, &res, "uploads");
                            } else if (req.method == HTTP_GET) {
                                handle_static_file(&req, &res, config->document_root);
                            } else {
                                res.status_code = 405;
                                res.status_message = "Method Not Allowed";
                                res.body = strdup("Method Not Allowed");
                                res.body_length = strlen(res.body);
                                http_add_header(&res, "Content-Type", "text/plain");
                            }
                        } else {
                            res.status_code = 400;
                            res.status_message = "Bad Request";
                            res.body = strdup("Bad Request");
                            res.body_length = strlen(res.body);
                            http_add_header(&res, "Content-Type", "text/plain");
                        }

                        char content_length_str[32];
                        snprintf(content_length_str, sizeof(content_length_str), "%d", res.body_length);
                        http_add_header(&res, "Content-Length", content_length_str);
                        http_add_header(&res, "Connection", "close");

                        int res_len;
                        char *res_str = http_build_response(&res, &res_len);
                        if (res.body) free(res.body);
                        
                        clients[i].write_buffer = res_str;
                        clients[i].write_len = res_len;
                        clients[i].write_pos = 0;
                        clients[i].file_fd = res.file_fd;
                        clients[i].state = STATE_WRITING;
                    }
                } else if (bytes_read == 0 || (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    // Disconnected or error
                    close(fd);
                    clients[i].state = STATE_DISCONNECTED;
                    if (clients[i].write_buffer) free(clients[i].write_buffer);
                    if (clients[i].file_fd >= 0) close(clients[i].file_fd);
                    printf("Client disconnected (fd=%d)\n", fd);
                }
            }

            if (clients[i].state == STATE_WRITING && FD_ISSET(fd, &write_fds)) {
                if (clients[i].write_pos < clients[i].write_len) {
                    int bytes_written = write(fd, clients[i].write_buffer + clients[i].write_pos, clients[i].write_len - clients[i].write_pos);
                    if (bytes_written > 0) {
                        clients[i].write_pos += bytes_written;
                    } else if (bytes_written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        close(fd);
                        clients[i].state = STATE_DISCONNECTED;
                        free(clients[i].write_buffer);
                        if (clients[i].file_fd >= 0) close(clients[i].file_fd);
                        printf("Write error, connection closed (fd=%d)\n", fd);
                    }
                } else if (clients[i].file_fd >= 0) {
                    char file_buf[8192];
                    int file_bytes = read(clients[i].file_fd, file_buf, sizeof(file_buf));
                    if (file_bytes > 0) {
                        int bytes_written = write(fd, file_buf, file_bytes);
                        if (bytes_written > 0 && bytes_written < file_bytes) {
                            lseek(clients[i].file_fd, bytes_written - file_bytes, SEEK_CUR);
                        } else if (bytes_written < 0 && errno == EAGAIN) {
                            lseek(clients[i].file_fd, -file_bytes, SEEK_CUR);
                        } else if (bytes_written < 0 && errno != EAGAIN) {
                            close(fd);
                            clients[i].state = STATE_DISCONNECTED;
                            free(clients[i].write_buffer);
                            close(clients[i].file_fd);
                            printf("Write error during stream, connection closed (fd=%d)\n", fd);
                        }
                    } else {
                        // EOF or error
                        close(fd);
                        clients[i].state = STATE_DISCONNECTED;
                        free(clients[i].write_buffer);
                        close(clients[i].file_fd);
                        printf("Finished writing stream, connection closed (fd=%d)\n", fd);
                    }
                } else {
                    // Done writing
                    close(fd);
                    clients[i].state = STATE_DISCONNECTED;
                    free(clients[i].write_buffer);
                    printf("Finished writing, connection closed (fd=%d)\n", fd);
                }
            }
        }
    }
}
