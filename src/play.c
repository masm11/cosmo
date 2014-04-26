#include <stdio.h>
#include <stdlib.h>
#include "map.h"
#include "screen.h"
#include "in.h"

#ifndef COUNTOF
# define COUNTOF(a) (sizeof(a) / sizeof(a)[0])
#endif

#define BALL_SPEED 64

static struct play_t {
    struct screen_t *scr;
    
    int level;
    
    int step;
    int timer, beg_timer;
    int fix_timer;
    
    int op_x, op_y;
    struct op_t {
	struct map_data_t data[2][2];
    } op, op_next;
    
    struct {
	int next_ball_dir;
	int ctr_to_ball;
    } seq;
    
    struct {
	int step, ctr;
	struct {
	    int l, r;
	} hist[MAP_HEIGHT][MAP_WIDTH];
	int dir, x, y, dx, dy;
    } ball;
    
    struct {
	struct flick_gang_t {
	    int used;
	    int x, y, dir;
	    int ctr;
	} gang[MAP_WIDTH * MAP_HEIGHT];
    } flick;
    int flick_num, erase_num;
    
    struct map_t map;
} play_w;

static int step_operating(struct play_t *w);
static int step_falling(struct play_t *w);
static int step_erase_blocks(struct play_t *w);
static int step_balling(struct play_t *w);
static int composable(struct map_t *map, struct op_t *op, int op_x, int op_y);
static void compose(struct map_t *map, struct op_t *op, int op_x, int op_y);
static void create_next_op(struct play_t *w);
static int check_game_over(struct play_t *w);
static void flick_gang_add(struct play_t *w, int x, int y, int dir);

void play_init(struct screen_t *scr)
{
    struct play_t *w = &play_w;
    
    memset(w, 0, sizeof *w);
    
    w->scr = scr;
    
    for (int i = 0; i < MAP_HEIGHT; i++) {
	for (int j = 0; j < MAP_WIDTH; j++)
	    w->map.data[i][j].type = TYPE_NONE;
    }
    
    w->seq.next_ball_dir = 1;
    w->seq.ctr_to_ball = 3;
    create_next_op(w);
}

void play_step(struct screen_t *scr)
{
    struct play_t *w = &play_w;
    int compose_op = 0;
    
    for (int y = 0; y < MAP_HEIGHT; y++) {
	for (int x = 0; x < MAP_WIDTH; x++)
	    w->map.data[y][x].ctr++;
    }
    for (int y = 0; y < 2; y++) {
	for (int x = 0; x < 2; x++)
	    w->op.data[y][x].ctr++;
    }
    for (int i = 0; i < COUNTOF(w->flick.gang); i++)
	w->flick.gang[i].ctr++;
    
    switch (w->step) {
    case 0:
	if (in_chk(IN_START, IN_ON))
	    w->step++;
	break;
	
    case 1:
	if (check_game_over(w)) {
	    w->step = 99;
	    break;
	}
	w->timer = 30;
	w->step++;
	break;
	
    case 2:	// �֥�å��и��ޤǤξ����Ԥ�
	if (--w->timer <= 0) {
	    w->op = w->op_next;
	    create_next_op(w);
	    w->op_x = MAP_WIDTH / 2 - 1;
	    w->op_y = 0;
	    w->step++;
	    w->timer = w->beg_timer = 1;
	}
	break;
	
    case 3:	// ���&�����
	compose_op = 1;
	if (step_operating(w))
	    w->step++;
	break;
	
    case 4:	// ʬ�򤷤����
	if (step_falling(w))
	    w->step++;
	break;
	
    case 5:	// �֥�å����ä��롣
	if (step_erase_blocks(w))
	    w->step++;
	break;
	
    case 6:	// �ä���ʬ����������롣
	if (step_falling(w)) {
	    memset(&w->ball, 0, sizeof w->ball);
	    w->step++;
	}
	break;
	
    case 7:	// ž����
	if (step_balling(w))
	    w->step++;
	break;
	
    case 8:	// �
	if (step_falling(w))
	    w->step++;
	break;
	
    case 9:	// �ä���
	if (step_erase_blocks(w))
	    w->step++;
	break;
	
    case 10:	// �
	if (step_falling(w))
	    w->step = 1;
	break;
	
    case 99:	// game over
	screen_set_game_over(scr);
	break;
    }
    
    struct map_t map;
    map = w->map;
    if (compose_op)
	compose(&map, &w->op, w->op_x, w->op_y);
    if (w->op_y >= 2)
	compose(&map, &w->op_next, MAP_WIDTH / 2 - 1, 0);
    screen_set_map(scr, &map);
    
    for (int i = 0; i < COUNTOF(w->flick.gang); i++) {
	struct flick_gang_t *fgp = &w->flick.gang[i];
	if (fgp->used) {
	    if (screen_set_flick_gang(w->scr, fgp->x, fgp->y, fgp->dir, fgp->ctr))
		fgp->used = 0;
	}
    }
}

static int step_operating(struct play_t *w)
{
    int retval = 0;
    
    /* ����� */
    if (in_chk(IN_LEFT, IN_REP)) {
	if (composable(&w->map, &w->op, w->op_x - 1, w->op_y))
	    w->op_x--;
    }
    if (in_chk(IN_RIGHT, IN_REP)) {
	if (composable(&w->map, &w->op, w->op_x + 1, w->op_y))
	    w->op_x++;
    }
    if (in_chk(IN_DOWN, IN_REP)) {
	if (composable(&w->map, &w->op, w->op_x, w->op_y + 1))
	    w->op_y++;
	else
	    w->fix_timer++;
    }
    if (in_chk(IN_A, IN_ON)) {
	struct op_t op;
	op.data[0][0] = w->op.data[0][1];
	op.data[0][1] = w->op.data[1][1];
	op.data[1][1] = w->op.data[1][0];
	op.data[1][0] = w->op.data[0][0];
	if (composable(&w->map, &op, w->op_x, w->op_y))
	    w->op = op;
    }
    
    /* �Ǥޤ���֤ˤʤä���Ǥ�� */
    if (--w->fix_timer <= 0) {
	if (!composable(&w->map, &w->op, w->op_x, w->op_y + 1)) {
	    compose(&w->map, &w->op, w->op_x, w->op_y);
	    /* �Ǥ᤿�顢���֤���Ȥ����㤤���ʤ��Τǡ������� return��*/
	    return 1;
	}
    }
    
    /* ���֤���Ȥ� */
    if (--w->timer <= 0) {
	if (composable(&w->map, &w->op, w->op_x, w->op_y + 1)) {
	    w->op_y++;
	    w->timer = w->beg_timer;
	    w->fix_timer = 30;
	}
    }
    
    return 0;
}

static int step_falling(struct play_t *w)
{
    int something_oped = 0;
    for (int y = MAP_HEIGHT - 1 - 1; y >= 0; y--) {
	for (int x = 0; x < MAP_WIDTH; x++) {
	    if (w->map.data[y][x].type != TYPE_NONE) {
		if (w->map.data[y + 1][x].type == TYPE_NONE) {
		    w->map.data[y + 1][x] = w->map.data[y][x];
		    w->map.data[y][x].type = TYPE_NONE;
		    something_oped = 1;
		}
	    }
	}
    }
    if (!something_oped)
	return 1;
    return 0;
}

static int step_erase_blocks(struct play_t *w)
{
    for (int y = 0; y < MAP_HEIGHT; y++) {
	int all_block = 1;
	for (int x = 0; x < MAP_WIDTH; x++) {
	    if (w->map.data[y][x].type != TYPE_BLOCK)
		all_block = 0;
	}
	if (all_block) {
	    w->erase_num++;
	    for (int x = 0; x < MAP_WIDTH; x++)
		w->map.data[y][x].type = TYPE_NONE;
	    printf("erase.num=%d\n", w->erase_num);
	}
    }
    
    return 1;
}

static int step_balling(struct play_t *w)
{
    int retval = 0;
    
    switch (w->ball.step) {
    case 0:
	retval = 1;
	for (int y = 0; y < MAP_HEIGHT; y++) {
	    for (int x = 0; x < MAP_WIDTH; x++) {
		if (w->map.data[y][x].type == TYPE_BALL) {
		    w->map.data[y][x].type = TYPE_NONE;
		    w->ball.x = x;
		    w->ball.y = y;
		    w->ball.dir = w->map.data[y][x].ball_dir;
		    w->ball.step++;
		    retval = 0;
		}
	    }
	}
	break;
	
    case 1:
	/* ���������Ƥ�ʤ顢���ä��ذ�ư */
	if (w->ball.y + 1 < MAP_HEIGHT && w->map.data[w->ball.y + 1][w->ball.x].type != TYPE_BLOCK) {
	    w->ball.y++;
	    w->ball.dy = -256;
	    if (w->map.data[w->ball.y][w->ball.x].type == TYPE_GANG) {
		w->flick_num++;
		flick_gang_add(w, w->ball.x, w->ball.y, w->ball.dir);
		printf("flick.num=%d\n", w->flick_num);
	    }
	    w->map.data[w->ball.y][w->ball.x].type = TYPE_NONE;
	    w->ball.step++;
	} else {
	    /* �����ߤξ��򺣤θ������̲ᤷ�����Ȥ����뤫������å���
	     * ����ʤ顢�⤦�ɤ��ؤ�Ԥ��ʤ��Τǡ�����ǽ�λ��
	     */
	    if (w->ball.dir < 0) {
		if (w->ball.hist[w->ball.y][w->ball.x].l) {
		    retval = 1;
		    break;
		}
		w->ball.hist[w->ball.y][w->ball.x].l = 1;
	    } else {
		if (w->ball.hist[w->ball.y][w->ball.x].r) {
		    retval = 1;
		    break;
		}
		w->ball.hist[w->ball.y][w->ball.x].r = 1;
	    }
	    
	    /* ���ä��عԤ��뤫���ǧ��
	     * �Ԥ��ʤ��ʤ顢ȿž��
	     */
	    if (w->ball.dir < 0) {
		if (w->ball.x - 1 < 0 || w->map.data[w->ball.y][w->ball.x - 1].type == TYPE_BLOCK) {
		    w->ball.dir = 1;
		    break;
		}
	    } else {
		if (w->ball.x + 1 >= MAP_WIDTH || w->map.data[w->ball.y][w->ball.x + 1].type == TYPE_BLOCK) {
		    w->ball.dir = -1;
		    break;
		}
	    }
	    
	    /* ����ʤ�����ư */
	    w->ball.x += w->ball.dir;
	    w->ball.dx = -256 * w->ball.dir;
	    if (w->map.data[w->ball.y][w->ball.x].type == TYPE_GANG) {
		w->flick_num++;
		flick_gang_add(w, w->ball.x, w->ball.y, w->ball.dir);
		printf("flick.num=%d\n", w->flick_num);
	    }
	    w->map.data[w->ball.y][w->ball.x].type = TYPE_NONE;
	    w->ball.step++;
	}
	/* fall through */
	
    case 2:
	if (w->ball.dy < 0) {
	    if ((w->ball.dy += BALL_SPEED) > 0)
		w->ball.dy = 0;
	} else if (w->ball.dy > 0) {
	    if ((w->ball.dy -= BALL_SPEED) < 0)
		w->ball.dy = 0;
	}
	if (w->ball.dx < 0) {
	    if ((w->ball.dx += BALL_SPEED) > 0)
		w->ball.dx = 0;
	} else if (w->ball.dx > 0) {
	    if ((w->ball.dx -= BALL_SPEED) < 0)
		w->ball.dx = 0;
	}
	if (w->ball.dx == 0 && w->ball.dy == 0)
	    w->ball.step = 1;
	break;
    }
    
    screen_set_ball(w->scr, w->ball.dir, w->ball.x, w->ball.y, w->ball.dx, w->ball.dy);
    
    return retval;
}

static int composable(struct map_t *map, struct op_t *op, int op_x, int op_y)
{
    for (int dy = 0; dy < 2; dy++) {
	for (int dx = 0; dx < 2; dx++) {
	    int x = op_x + dx;
	    int y = op_y + dy;
	    
	    if (x < 0 || x >= MAP_WIDTH)
		return 0;
	    if (y < 0 || y >= MAP_HEIGHT)
		return 0;
	    
	    if (op->data[dy][dx].type != TYPE_NONE && map->data[y][x].type != TYPE_NONE)
		return 0;
	}
    }
    
    return 1;
}

static void compose(struct map_t *map, struct op_t *op, int op_x, int op_y)
{
    for (int dy = 0; dy < 2; dy++) {
	for (int dx = 0; dx < 2; dx++) {
	    int x = op_x + dx;
	    int y = op_y + dy;
	    
	    if (x < 0 || x >= MAP_WIDTH)
		continue;
	    if (y < 0 || y >= MAP_HEIGHT)
		continue;
	    
	    if (op->data[dy][dx].type != TYPE_NONE)
		map->data[y][x] = op->data[dy][dx];
	}
    }
}

static void create_next_op(struct play_t *w)
{
    w->op_next.data[0][0].type = (rand() & 1) ? TYPE_BLOCK : TYPE_GANG;
    w->op_next.data[0][1].type = (rand() & 1) ? TYPE_BLOCK : TYPE_GANG;
    w->op_next.data[1][0].type = (rand() & 1) ? TYPE_BLOCK : TYPE_GANG;
    
    if (--w->seq.ctr_to_ball <= 0) {
	w->op_next.data[0][0].type = TYPE_BALL;
	w->op_next.data[0][0].ball_dir = w->seq.next_ball_dir;
	w->seq.next_ball_dir = -w->seq.next_ball_dir;
	w->seq.ctr_to_ball = 3;
    }
}

static int check_game_over(struct play_t *w)
{
    for (int y = 0; y < 2; y++) {
	for (int x = 0; x < MAP_WIDTH; x++) {
	    if (w->map.data[y][x].type != TYPE_NONE)
		return 1;
	}
    }
    
    return 0;
}

static void flick_gang_add(struct play_t *w, int x, int y, int dir)
{
    for (int i = 0; i < COUNTOF(w->flick.gang); i++) {
	if (!w->flick.gang[i].used) {
	    w->flick.gang[i].x = x;
	    w->flick.gang[i].y = y;
	    w->flick.gang[i].dir = dir;
	    w->flick.gang[i].ctr = 0;
	    w->flick.gang[i].used = 1;
	    return;
	}
    }
}
