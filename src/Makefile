CC = gcc -std=gnu99
CFLAGS = 
OBJS = main.o screen_tty.o screen_x11.o game.o play.o

all: cgp

cgp: $(OBJS)
	$(CC) -o $@ $(OBJS) -L/usr/X11R6/lib -lXpm -lXext -lX11 -lcurses -lpthread

clean:
	rm -f *.o cgp
