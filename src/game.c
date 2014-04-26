#include <stdio.h>
#include <stdlib.h>

static struct game_t {
    int dummy;
} game_w;

void game_init(struct screen_t *scr)
{
    play_init(scr);
}

void game_step(struct screen_t *scr)
{
    play_step(scr);
}
