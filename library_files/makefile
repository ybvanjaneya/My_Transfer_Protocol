CC = gcc
CFLAGS = -Wall -Werror

# Target for building the static library
libmsocket.a: msocket.o
	ar rcs libmsocket.a msocket.o

# Rule for compiling source file into object file
msocket.o: msocket.c msocket.h
	$(CC) $(CFLAGS) -c msocket.c -o msocket.o

# Clean rule
clean:
	rm -f msocket.o libmsocket.a
