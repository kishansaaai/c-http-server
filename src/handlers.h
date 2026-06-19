#ifndef HANDLERS_H
#define HANDLERS_H

#include "http.h"

// Returns the correct MIME type based on the file extension
const char *get_mime_type(const char *path);

// Handles static file serving requests
void handle_static_file(const HttpRequest *req, HttpResponse *res, const char *document_root);

// Handles CGI scripts
void handle_cgi(const HttpRequest *req, HttpResponse *res, const char *cgi_root);

// Handles file uploads
void handle_upload(const HttpRequest *req, HttpResponse *res, const char *upload_dir);

#endif // HANDLERS_H
