#define _POSIX_C_SOURCE 200809L
#include "handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    
    return "application/octet-stream";
}

void handle_static_file(const HttpRequest *req, HttpResponse *res, const char *document_root) {
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s%s", document_root, req->path);
    
    // Default to index.html if a directory is requested
    if (file_path[strlen(file_path) - 1] == '/') {
        strncat(file_path, "index.html", sizeof(file_path) - strlen(file_path) - 1);
    }
    
    struct stat st;
    if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
        int fd = open(file_path, O_RDONLY);
        if (fd >= 0) {
            res->status_code = 200;
            res->status_message = "OK";
            
            res->body_length = st.st_size;
            res->file_fd = fd;
            
            const char *mime_type = get_mime_type(file_path);
            http_add_header(res, "Content-Type", mime_type);
        } else {
            res->status_code = 500;
            res->status_message = "Internal Server Error";
            res->body = strdup("500 Internal Server Error: Cannot read file.");
            res->body_length = strlen(res->body);
            http_add_header(res, "Content-Type", "text/plain");
        }
    } else {
        res->status_code = 404;
        res->status_message = "Not Found";
        res->body = strdup("404 Not Found");
        res->body_length = strlen(res->body);
        http_add_header(res, "Content-Type", "text/plain");
    }
}

void handle_cgi(const HttpRequest *req, HttpResponse *res, const char *cgi_root) {
    char script_path[1024];
    // req->path is like /cgi-bin/test.sh, skip the /cgi-bin prefix
    const char *script_name = req->path + strlen("/cgi-bin");
    snprintf(script_path, sizeof(script_path), "%s%s", cgi_root, script_name);

    if (access(script_path, X_OK) != 0) {
        res->status_code = 404;
        res->status_message = "Not Found";
        res->body = strdup("CGI Script Not Found or Not Executable");
        res->body_length = strlen(res->body);
        http_add_header(res, "Content-Type", "text/plain");
        return;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        res->status_code = 500;
        res->status_message = "Internal Server Error";
        res->body = strdup("Pipe failed");
        res->body_length = strlen(res->body);
        http_add_header(res, "Content-Type", "text/plain");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        res->status_code = 500;
        res->status_message = "Internal Server Error";
        res->body = strdup("Fork failed");
        res->body_length = strlen(res->body);
        http_add_header(res, "Content-Type", "text/plain");
        return;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]); // Close read end
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
        close(pipefd[1]);

        // Set environment variables
        if (req->method == HTTP_GET) setenv("REQUEST_METHOD", "GET", 1);
        else if (req->method == HTTP_POST) setenv("REQUEST_METHOD", "POST", 1);
        
        // Very basic query string parsing (if any)
        char *query = strchr(req->path, '?');
        if (query) {
            setenv("QUERY_STRING", query + 1, 1);
        } else {
            setenv("QUERY_STRING", "", 1);
        }

        const char *cookie = http_get_header(req, "Cookie");
        if (cookie) setenv("HTTP_COOKIE", cookie, 1);

        execl(script_path, script_path, NULL);
        exit(EXIT_FAILURE); // Only reached if execl fails
    } else {
        // Parent process
        close(pipefd[1]); // Close write end

        char buffer[4096];
        int total_read = 0;
        int capacity = 4096;
        char *output = malloc(capacity);

        while (1) {
            ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer));
            if (bytes_read <= 0) break;

            if (total_read + bytes_read > capacity) {
                capacity *= 2;
                output = realloc(output, capacity);
            }
            memcpy(output + total_read, buffer, bytes_read);
            total_read += bytes_read;
        }
        close(pipefd[0]);
        waitpid(pid, NULL, 0);

        // Basic parsing of CGI output (assumes script outputs headers then blank line then body)
        // For simplicity in Phase 5, we just treat the entire output as the response body
        // and set a generic Content-Type. A real CGI server parses the script's headers.
        
        char *header_end = strstr(output, "\r\n\r\n");
        if (!header_end) header_end = strstr(output, "\n\n");
        
        if (header_end) {
            // Script provided headers. We should ideally parse them, but for now just send body
            int header_len = (header_end[0] == '\r') ? 4 : 2;
            int body_len = total_read - (header_end - output) - header_len;
            res->body = malloc(body_len);
            memcpy(res->body, header_end + header_len, body_len);
            res->body_length = body_len;
            
            // Try to extract Content-Type if present
            // (Skipping robust header parsing of CGI output for brevity)
            http_add_header(res, "Content-Type", "text/html");
            free(output);
        } else {
            res->body = output;
            res->body_length = total_read;
            http_add_header(res, "Content-Type", "text/plain");
        }

        res->status_code = 200;
        res->status_message = "OK";
    }
}

void handle_upload(const HttpRequest *req, HttpResponse *res, const char *upload_dir) {
    if (req->method != HTTP_POST) {
        res->status_code = 405;
        res->status_message = "Method Not Allowed";
        res->body = strdup("Only POST is allowed for uploads.");
        res->body_length = strlen(res->body);
        http_add_header(res, "Content-Type", "text/plain");
        return;
    }

    const char *content_type = http_get_header(req, "Content-Type");
    if (!content_type || !strstr(content_type, "multipart/form-data")) {
        res->status_code = 400;
        res->status_message = "Bad Request";
        res->body = strdup("Content-Type must be multipart/form-data.");
        res->body_length = strlen(res->body);
        http_add_header(res, "Content-Type", "text/plain");
        return;
    }

    // A very basic parser for multipart/form-data that finds filename="..." and extracts the file
    // Note: For a robust server, use a state machine parser.
    const char *filename_attr = strstr(req->body, "filename=\"");
    if (!filename_attr) {
        res->status_code = 400;
        res->status_message = "Bad Request";
        res->body = strdup("No filename found in multipart data.");
        res->body_length = strlen(res->body);
        http_add_header(res, "Content-Type", "text/plain");
        return;
    }

    filename_attr += strlen("filename=\"");
    char filename[256];
    int i = 0;
    while (*filename_attr != '"' && i < 255) {
        filename[i++] = *filename_attr++;
    }
    filename[i] = '\0';

    // Find the end of the multipart headers
    const char *file_data_start = strstr(filename_attr, "\r\n\r\n");
    if (!file_data_start) {
        res->status_code = 400;
        res->status_message = "Bad Request";
        res->body = strdup("Malformed multipart data.");
        res->body_length = strlen(res->body);
        http_add_header(res, "Content-Type", "text/plain");
        return;
    }
    file_data_start += 4; // Skip \r\n\r\n

    // The boundary ends with \r\n--boundary--
    // We do a simple search for \r\n--
    const char *file_data_end = strstr(file_data_start, "\r\n--");
    if (!file_data_end) {
        res->status_code = 400;
        res->status_message = "Bad Request";
        res->body = strdup("End boundary not found.");
        res->body_length = strlen(res->body);
        http_add_header(res, "Content-Type", "text/plain");
        return;
    }

    int file_len = file_data_end - file_data_start;

    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/%s", upload_dir, filename);

    FILE *fp = fopen(file_path, "wb");
    if (!fp) {
        res->status_code = 500;
        res->status_message = "Internal Server Error";
        res->body = strdup("Failed to save file.");
        res->body_length = strlen(res->body);
        http_add_header(res, "Content-Type", "text/plain");
        return;
    }
    fwrite(file_data_start, 1, file_len, fp);
    fclose(fp);

    res->status_code = 200;
    res->status_message = "OK";
    char response_msg[512];
    snprintf(response_msg, sizeof(response_msg), "File '%s' uploaded successfully (%d bytes).", filename, file_len);
    res->body = strdup(response_msg);
    res->body_length = strlen(res->body);
    http_add_header(res, "Content-Type", "text/plain");
}
