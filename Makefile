CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11 -D_XOPEN_SOURCE=700
TARGET := context_switch_lab

.PHONY: all run clean 

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)