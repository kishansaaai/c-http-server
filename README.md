# C HTTP Server

A lightweight, fully functional HTTP/1.1 server written from scratch in C for POSIX/Linux environments. 

## Features

- **Static File Serving**: Serves static assets (`.html`, `.css`, `.js`, `.png`, `.jpg`, `.txt`) with correct MIME types.
- **CGI (Common Gateway Interface)**: Executes CGI scripts and pipes their output directly to the HTTP response.
- **File Uploads**: Supports `multipart/form-data` parsing for uploading files directly to the server.
- **Configurable**: Driven by a simple `server.conf` file to configure ports and directory roots.
- **Non-blocking / Signal Handling**: Ignores `SIGPIPE` to prevent crashes on premature client disconnects.

## Project Structure

- `src/` - Contains the C source code.
  - `main.c` - Entry point, configuration loading, and server initialization.
  - `server.c` / `server.h` - Core socket programming, connection handling, and event loop.
  - `http.c` / `http.h` - HTTP request parsing and response formatting.
  - `handlers.c` / `handlers.h` - Logic for static files, CGI execution, and file uploads.
- `www/` - The document root for serving static files.
- `cgi-bin/` - Directory for executable CGI scripts.
- `uploads/` - Destination directory for uploaded files.
- `server.conf` - Configuration file.

## Requirements

This project relies on POSIX-specific system calls (`<sys/wait.h>`, `<arpa/inet.h>`, `fork()`, `pipe()`). 
It requires a Linux environment or Windows Subsystem for Linux (WSL) to compile and run.

- GCC or Clang compiler
- Make (optional, but a `Makefile` is provided)

## Building the Server

To build the project, simply use `make` in the root directory:

```bash
make
```

This will compile the source files and generate an executable named `http_server`.

## Running the Server

Start the server by executing the compiled binary:

```bash
./http_server
```

The server will read `server.conf` to determine the listening port and root directories.

## Configuration (`server.conf`)

The server is configured using a simple key-value format in `server.conf`:

```
port=8080
document_root=./www
cgi_root=./cgi-bin
```

## Supported Endpoints

- `GET /` -> Serves `index.html` from the document root.
- `GET /*` -> Serves static files from the document root.
- `GET /cgi-bin/*` or `POST /cgi-bin/*` -> Executes the specified script in the CGI root.
- `POST /upload` -> Handles `multipart/form-data` uploads and saves them to the `uploads/` directory.

## Known Limitations

- **Buffer Size**: The server reads incoming requests into a fixed-size `BUFFER_SIZE` array (64KB). Requests or uploads larger than this limit are not properly handled because the read loop expects the full request to fit into this initial buffer.

## License

MIT
