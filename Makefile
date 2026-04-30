CC = gcc
CFLAGS = -Wall -std=c99
LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
NAME = flower

S = main.c

$(NAME): $(S)
	$(CC) $(CFLAGS) $(S) -o $(NAME) $(LDFLAGS)

clean:
	rm -f $(NAME)
