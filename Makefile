CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11 -D_XOPEN_SOURCE=700
TARGET := context_switch_lab
IMAGE := context-switch-lab

.PHONY: all run clean docker-build docker-run docker-rebuild

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

docker-build:
	docker build -t $(IMAGE) .

docker-run:
	docker run --rm -it $(IMAGE)

docker-rebuild: docker-build docker-run