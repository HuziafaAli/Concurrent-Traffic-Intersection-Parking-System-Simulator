CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
LDFLAGS = -pthread -lrt -lm -lSDL2 -lSDL2_ttf

SRC = main.c controllers.c vehicles.c graphics.c
OBJ = $(SRC:.c=.o)
EXEC = traffic_simulation

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)
	rm -f $(OBJ)

%.o: %.c simulation.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean
