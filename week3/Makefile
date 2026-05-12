CC      = clang
CFLAGS  = -Wall -std=c99 $(shell pkg-config --cflags raylib)
NAME    = app
S       = main.c
PLANT   = plant
PLANT2  = plant2
ROCKS   = rocks

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    LDFLAGS = $(shell pkg-config --libs raylib) -lm \
              -framework CoreVideo -framework IOKit -framework Cocoa \
              -framework GLUT -framework OpenGL
else
    LDFLAGS = $(shell pkg-config --libs raylib) -lm -lpthread -ldl
endif

all: $(NAME) $(PLANT) $(PLANT2) $(ROCKS)

$(NAME): $(S)
	$(CC) $(CFLAGS) $(S) -o $(NAME) $(LDFLAGS)

$(PLANT): plant.c
	$(CC) $(CFLAGS) plant.c -o $(PLANT) $(LDFLAGS)

$(PLANT2): plant2.c
	$(CC) $(CFLAGS) -O2 plant2.c -o $(PLANT2) $(LDFLAGS)

$(ROCKS): rocks.c
	$(CC) $(CFLAGS) -O2 rocks.c -o $(ROCKS) $(LDFLAGS)

clean:
	rm -f $(NAME) $(PLANT) $(PLANT2) $(ROCKS)
