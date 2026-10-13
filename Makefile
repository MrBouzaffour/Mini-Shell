CC = gcc
CFLAGS = -std=gnu90 -Wall -Wextra -pedantic -g -O0
TARGET = mini-shell
SRC = shell.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)
