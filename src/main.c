#include <stdio.h>
#include <stdlib.h>
#include "screen.h"
#include "in.h"

static struct screen_t *scr;

struct in_t {
    unsigned long old;
    unsigned long now;
    int repctr[IN_NR];
} in;

void in_update(struct screen_t *scr)
{
    in.old = in.now;
    in.now = screen_in(scr);
    for (int i = 0; i < IN_NR; i++) {
	if (in.now & (1 << i)) {
	    in.repctr[i]++;
	} else {
	    in.repctr[i] = 0;
	}
    }
}

int in_chk(int key, int type)
{
    if (key < 0)
	return 0;
    if (key >= IN_NR)
	return 0;
    
    int f = 0;
    switch (type) {
    case IN_NOW:
	f = in.now & (1 << key);
	break;
    case IN_ON:
	f = in.now & ~in.old & (1 << key);
	break;
    case IN_OFF:
	f = ~in.now & in.old & (1 << key);
	break;
    case IN_REP:
	f = (in.now & (1 << key)) && ((in.repctr[key] >= 20) || (~in.old & (1 << key)));
	break;
    }
    
    return !!f;
}

void main_init(int *argcp, char **argv)
{
//    scr = (*screen_tty_ops.new)(argcp, argv);
    scr = (*screen_x11_ops.new)(argcp, argv);
    game_init(scr);
}

static int next_notify = 0;
void main_loop(void)
{
    if (next_notify)
	screen_notify(scr);
    next_notify = 0;
    
    if (screen_do_frame(scr)) {
	in_update(scr);
	game_step(scr);
	next_notify = 1;
    }
}

int main(int argc, char **argv)
{
    main_init(&argc, argv);
    
    while (1) {
	main_loop();
	usleep(16667);
    }
    
    return 0;
}
