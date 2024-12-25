CC = gcc

SRC = src/*.c
EXE = bin/rukchess

WFLAGS = -Wall -Wextra -Wshadow -Wno-unused-result
CFLAGS = $(WFLAGS) -O3 -flto=auto -fopenmp -DNDEBUG -march=native
LDFLAGS = -Wl,-z,stack-size=10485760

LIBS = -lm -pthread

all:
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) $(LIBS) -o $(EXE)