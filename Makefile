CC = gcc
CFLAGS = -Wall -Wextra -Werror -g

TARGET = server.out
OBJDIR = build/

SRC = main.c server.c buffer.c
OBJ = $(SRC:%.c=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	mkdir -p $(dir $(OBJ))
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

$(OBJDIR)/%.o: %.c
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -rf build

re: clean all

.PHONY: all run clean re