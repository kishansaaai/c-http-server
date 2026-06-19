#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define MAX_HEADERS 64

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_DELETE,
    HTTP_UNKNOWN
} HttpMethod;

typedef struct {
    char *name;
    char *value;
} HttpHeader;

typedef struct {
    HttpMethod method;
    char *path;
    char *version;
    HttpHeader headers[MAX_HEADERS];
    int header_count;
    char *body;
} HttpRequest;

typedef struct {
    int status_code;
    const char *status_message;
    HttpHeader headers[MAX_HEADERS];
    int header_count;
    char *body;
    int body_length;
    int file_fd; // For streaming large files
} HttpResponse;

// Parses raw string data into an HttpRequest structure.
// Returns 0 on success, -1 on failure.
int http_parse_request(char *raw_request, HttpRequest *req);

// Helper to get a header value by name
const char *http_get_header(const HttpRequest *req, const char *name);

// Helper to init response
void http_init_response(HttpResponse *res);

// Helper to add header to response
void http_add_header(HttpResponse *res, const char *name, const char *value);

// Build raw response string from HttpResponse structure.
// Caller must free the returned string.
char *http_build_response(const HttpResponse *res, int *out_len);

#endif // HTTP_H
