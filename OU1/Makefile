CC = gcc
CFLAGS = -g -std=gnu11 -Werror -Wall -Wextra -Wpedantic -Wmissing-declarations -Wmissing-prototypes -Wold-style-definition

TARGET = mexec
OBJ = mexec.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

mexec.o: mexec.c
	$(CC) $(CFLAGS) -c mexec.c

clean:
	rm -f $(TARGET) $(OBJ)
