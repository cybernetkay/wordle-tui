CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -D_DEFAULT_SOURCE

TARGET = wordle

SRC = main.c


all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

# Evita conflitti se hai file chiamati "clean" o "run" nella cartella
.PHONY: all clean run
