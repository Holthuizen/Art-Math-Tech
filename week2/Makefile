CC      = clang
CFLAGS  = -Wall -std=c99 $(shell pkg-config --cflags raylib)
NAME    = chips_app
S       = chips/main.c

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    LDFLAGS = $(shell pkg-config --libs raylib) -lm \
              -framework CoreVideo -framework IOKit -framework Cocoa \
              -framework GLUT -framework OpenGL
else
    LDFLAGS = $(shell pkg-config --libs raylib) -lm -lpthread -ldl
endif

$(NAME): $(S)
	$(CC) $(CFLAGS) $(S) -o $(NAME) $(LDFLAGS)

clean:
	rm -f $(NAME)
