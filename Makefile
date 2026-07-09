CC=gcc
CFLAGS=-Wall -Wextra -O2

TARGET=vmctl.exe

all: $(TARGET)

$(TARGET): vmctl.c
	$(CC) $(CFLAGS) -o $(TARGET) vmctl.c

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

clean:
	rm -f $(TARGET)

.PHONY: all install clean
