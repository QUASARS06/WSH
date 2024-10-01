LOGIN = chiragjain
SUBMITPATH = ~cs537-1/handin/$(LOGIN)/p3
CC = gcc
CFLAGS-common = -Wall -Wextra -Werror -pedantic -std=gnu18
CFLAGS = $(CFLAGS-common) -O2
CFLAGS-dbg = $(CFLAGS-common) -Og -ggdb
TARGET = wsh
SRC = $(TARGET).c $(TARGET).h

all: $(TARGET) $(TARGET)-dbg

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

$(TARGET)-dbg: $(SRC)
	$(CC) $(CFLAGS-dbg) $< -o $@

clean:
	rm -f $(TARGET) $(TARGET)-dbg local*.txt