#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curses.h>
#include "map.h"
#include "screen.h"

struct screen_in_t {
    int up, down, left, right;
    int a;
};

struct screen_tty_t {
    const struct screen_ops_t *ops;
    
    pthread_t thread;
    
    struct {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int ev;
    } event;
    
    struct map_t map;
    struct screen_in_t in;
    
    struct {
	struct map_t map;
	struct screen_in_t in;
    } frame;
};

static void *screen_tty_thread(void *parm);

static struct screen_t *screen_tty_new(int *argcp, char **argv)
{
    struct screen_tty_t *w = malloc(sizeof *w);
    
    memset(w, 0, sizeof *w);
    
    w->ops = &screen_tty_ops;
    
    pthread_mutex_init(&w->event.mutex, NULL);
    pthread_cond_init(&w->event.cond, NULL);
    
    pthread_create(&w->thread, NULL, screen_tty_thread, w);
    
    return (struct screen_t *) w;
}

static void *screen_tty_thread(void *parm)
{
    struct screen_tty_t *w = parm;
    
    initscr();
    cbreak();
    noecho();
    timeout(0);
    
    nonl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    
    while (1) {
	pthread_mutex_lock(&w->event.mutex);
	
	while (!w->event.ev)
	    pthread_cond_wait(&w->event.cond, &w->event.mutex);
	w->event.ev = 0;
	
	pthread_mutex_unlock(&w->event.mutex);
	
	while (1) {
	    int ch = getch();
	    if (ch == ERR)
		break;
	    switch (ch) {
	    case KEY_UP:
		w->in.up = 1;
		break;
	    case KEY_DOWN:
		w->in.down = 1;
		break;
	    case KEY_RIGHT:
		w->in.right = 1;
		break;
	    case KEY_LEFT:
		w->in.left = 1;
		break;
	    case 'z':
		w->in.a = 1;
		break;
	    }
	}
	
	for (int y = 0; y < MAP_HEIGHT; y++) {
	    for (int x = 0; x < MAP_WIDTH; x++) {
		int c = ' ';
		switch (w->map.data[y][x].type) {
		case TYPE_NONE:
		    break;
		case TYPE_BLOCK:
		    c = '*';
		    break;
		case TYPE_GANG:
		    c = 'o';
		    break;
		case TYPE_BALL:
		    if (w->map.data[y][x].ball_dir < 0)
			c = '<';
		    else
			c = '>';
		    break;
		}
		mvprintw(y, x, "%c", c);
	    }
	}
	
	refresh();
    }
    
    return NULL;
}

static void screen_tty_notify(struct screen_t *scr)
{
    struct screen_tty_t *w = (struct screen_tty_t *) scr;
    
    pthread_mutex_lock(&w->event.mutex);
    
    w->event.ev = 1;
    pthread_cond_signal(&w->event.cond);
    
    w->map = w->frame.map;
    
    w->frame.in = w->in;
    memset(&w->in, 0, sizeof w->in);
    
    pthread_mutex_unlock(&w->event.mutex);
}

static void screen_tty_set_map(struct screen_t *scr, const struct map_t *map)
{
    struct screen_tty_t *w = (struct screen_tty_t *) scr;
    
    w->frame.map = *map;
}

static unsigned long screen_tty_in(struct screen_t *scr)
{
    struct screen_tty_t *w = (struct screen_tty_t *) scr;
    
    return w->frame.in.a * SCREEN_IN_A |
	    w->frame.in.up * SCREEN_IN_UP |
	    w->frame.in.down * SCREEN_IN_DOWN |
	    w->frame.in.left * SCREEN_IN_LEFT |
	    w->frame.in.right * SCREEN_IN_RIGHT;
}

struct screen_ops_t screen_tty_ops = {
    .new = screen_tty_new,
    .notify = screen_tty_notify,
    .set_map = screen_tty_set_map,
    .in = screen_tty_in,
};
