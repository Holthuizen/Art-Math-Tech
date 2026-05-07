CC     = gcc
CFLAGS = -Wall -std=c99 $(shell pkg-config --cflags raylib)
LDFLAGS = $(shell pkg-config --libs raylib) -lm -lpthread -ldl
NAME   = chips


S = chips/main.c

$(NAME): $(S)
	$(CC) $(CFLAGS) $(S) -o $(NAME) $(LDFLAGS)

clean:
	rm -f $(NAME)
