CC := gcc
CFLAGS := -Wall -Wextra -std=c11 $(shell pkg-config --cflags gtk4)
LDFLAGS := $(shell pkg-config --libs gtk4)
TARGET := calendario
SRC := src/main.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
