CC = gcc
CFLAGS = -Wall -Wextra -std=c11

SRC = src/main.c src/server.c src/http.c src/handlers.c
OBJ = $(SRC:.c=.o)
TARGET = http_server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
