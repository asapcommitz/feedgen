CC = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags libcurl libxml-2.0)
LDFLAGS = $(shell pkg-config --libs libcurl libxml-2.0)
TARGET = feedgen
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
