CC = clang
BREW_PREFIX := $(shell brew --prefix)
CFLAGS = -Wall -std=c99 -I$(BREW_PREFIX)/include
LDFLAGS = -L$(BREW_PREFIX)/lib -lraylib -lm -lpthread -ldl -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL
NAME = flower

S = main.c

$(NAME): $(S)
	$(CC) $(CFLAGS) $(S) -o $(NAME) $(LDFLAGS)

clean:
	rm -f $(NAME)
