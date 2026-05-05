CC = gcc
CFLAGS = -Wall -Wextra -Werror -g

TARGET = server.out

SRC = main.c server.c buffer.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJ)

re: clean all

.PHONY: all run clean re