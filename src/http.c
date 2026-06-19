#define _POSIX_C_SOURCE 200809L
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int http_parse_request(char *raw_request, HttpRequest *req) {
    memset(req, 0, sizeof(HttpRequest));
    
    char *header_end = strstr(raw_request, "\r\n\r\n");
    if (!header_end) return -1;
    
    *header_end = '\0'; // Terminate headers section
    req->body = header_end + 4;
    
    char *saveptr;
    // Extract the first line
    char *line = strtok_r(raw_request, "\r\n", &saveptr);
    if (!line) return -1;

    // Parse method
    char *method_str = strtok(line, " ");
    if (!method_str) return -1;

    if (strcmp(method_str, "GET") == 0) req->method = HTTP_GET;
    else if (strcmp(method_str, "POST") == 0) req->method = HTTP_POST;
    else if (strcmp(method_str, "DELETE") == 0) req->method = HTTP_DELETE;
    else req->method = HTTP_UNKNOWN;

    // Parse path
    char *path_str = strtok(NULL, " ");
    if (!path_str) return -1;
    req->path = path_str;

    // Parse version
    char *version_str = strtok(NULL, " ");
    if (!version_str) return -1;
    req->version = version_str;

    // Parse headers
    while ((line = strtok_r(NULL, "\r\n", &saveptr)) != NULL) {
        char *colon = strchr(line, ':');
        if (colon && req->header_count < MAX_HEADERS) {
            *colon = '\0';
            req->headers[req->header_count].name = line;
            
            char *value = colon + 1;
            while (*value == ' ') value++; // skip leading spaces
            req->headers[req->header_count].value = value;
            
            req->header_count++;
        }
    }

    return 0;
}

const char *http_get_header(const HttpRequest *req, const char *name) {
    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i].name, name) == 0) {
            return req->headers[i].value;
        }
    }
    return NULL;
}

void http_init_response(HttpResponse *res) {
    memset(res, 0, sizeof(HttpResponse));
    res->status_code = 200;
    res->status_message = "OK";
    res->file_fd = -1;
}

void http_add_header(HttpResponse *res, const char *name, const char *value) {
    if (res->header_count < MAX_HEADERS) {
        // Here we duplicate strings because we might pass literals
        res->headers[res->header_count].name = strdup(name);
        res->headers[res->header_count].value = strdup(value);
        res->header_count++;
    }
}

char *http_build_response(const HttpResponse *res, int *out_len) {
    // A simple builder, ideally this should be dynamic
    int capacity = 4096;
    if (res->body_length > 0) {
        capacity += res->body_length;
    }
    char *buffer = malloc(capacity);
    if (!buffer) return NULL;

    int offset = snprintf(buffer, capacity, "HTTP/1.1 %d %s\r\n", res->status_code, res->status_message);

    for (int i = 0; i < res->header_count; i++) {
        offset += snprintf(buffer + offset, capacity - offset, "%s: %s\r\n", res->headers[i].name, res->headers[i].value);
        free(res->headers[i].name);
        free(res->headers[i].value);
    }

    offset += snprintf(buffer + offset, capacity - offset, "\r\n");

    if (res->body && res->body_length > 0) {
        memcpy(buffer + offset, res->body, res->body_length);
        offset += res->body_length;
    }

    *out_len = offset;
    return buffer;
}
