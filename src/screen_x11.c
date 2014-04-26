#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/Xdbe.h>
#include <X11/xpm.h>
#include "map.h"
#include "screen.h"

#ifndef COUNTOF
# define COUNTOF(a) (sizeof(a) / sizeof(a)[0])
#endif

struct screen_in_t {
    int up, down, left, right;
    int a;
    int start;
    
    int pause, frame;
};

struct screen_x11_t {
    const struct screen_ops_t *ops;
    
    Display *dpy;
    Screen *scr;
    Window root;
    Depth *depth;
    Visual *visual;
    Colormap cmap;
    XColor colors[5];
    XColor *white;
    XColor *black;
    XColor *red;
    XColor *green;
    XColor *blue;
    Window win;
    XdbeBackBuffer backbuf;
    struct {
	struct chara_t {
	    Pixmap pix;
	    Pixmap mask;
	} block, ball_r, ball_l, gang, gang_l, gang_r, gang_d;
    } pix;
    
    pthread_t thread;
    
    struct {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int ev;
    } event;
    
    int ball_dir, ball_x, ball_y, ball_dx, ball_dy;
    struct map_t map;
    int disp_game_over;
    struct screen_in_t in;
    struct flick_t {
	struct flick_gang_t {
	    int used, x, y, dir, ctr;
	} gangs[16];
    } flick;
    
    struct {
	int ball_dir, ball_x, ball_y, ball_dx, ball_dy;
	struct map_t map;
	int disp_game_over;
	struct flick_t flick;
    } frame;
    
    struct {
	int last_pause, last_frame;
	int pausing;
    } pause;
};

static void *screen_x11_thread_out(void *parm);
static void *screen_x11_thread_in(void *parm);
static void draw_ball(struct screen_x11_t *w, Display *dpy, Window win, GC gc,
	int dir, int x, int y, int dx, int dy);
static void draw_gang(struct screen_x11_t *w, Display *dpy, Drawable win, GC gc,
	int x, int y, int ctr);
static void select_visual(struct screen_x11_t *w);
static void read_images(struct screen_x11_t *w);

static struct screen_t *screen_x11_new(int *argcp, char **argv)
{
    struct screen_x11_t *w = malloc(sizeof *w);
    
    memset(w, 0, sizeof *w);
    
    w->ops = &screen_x11_ops;
    
    pthread_mutex_init(&w->event.mutex, NULL);
    pthread_cond_init(&w->event.cond, NULL);
    
    w->dpy = XOpenDisplay(NULL);
    w->scr = DefaultScreenOfDisplay(w->dpy);
    w->root = RootWindowOfScreen(w->scr);
    
    select_visual(w);
    
    w->cmap = XCreateColormap(w->dpy, w->root, w->visual, False);
    
    w->colors[0] = (XColor) {
	.red = 0xffff,
	.green = 0xffff,
	.blue = 0xffff,
	.flags = DoRed | DoGreen | DoBlue,
    };
    w->colors[1] = (XColor) {
	.red = 0x0000,
	.green = 0x0000,
	.blue = 0x0000,
	.flags = DoRed | DoGreen | DoBlue,
    };
    w->colors[2] = (XColor) {
	.red = 0xffff,
	.green = 0x0000,
	.blue = 0x0000,
	.flags = DoRed | DoGreen | DoBlue,
    };
    w->colors[3] = (XColor) {
	.red = 0x0000,
	.green = 0xffff,
	.blue = 0x0000,
	.flags = DoRed | DoGreen | DoBlue,
    };
    w->colors[4] = (XColor) {
	.red = 0x0000,
	.green = 0x0000,
	.blue = 0xffff,
	.flags = DoRed | DoGreen | DoBlue,
    };
    w->white = &w->colors[0];
    w->black = &w->colors[1];
    w->red = &w->colors[2];
    w->green = &w->colors[3];
    w->blue = &w->colors[4];
    for (int i = 0; i < 5; i++) {
	if (!XAllocColor(w->dpy, w->cmap, &w->colors[i])) {
	    fprintf(stderr, "can't alloc color.\n");
	    exit(1);
	}
    }
    
    w->win = XCreateWindow(
	    w->dpy, w->root,
	    0, 0, 256, 512, 0,
	    w->depth->depth, InputOutput, w->visual,
	    CWBackPixel | CWColormap,
	    &(XSetWindowAttributes) {
		.background_pixel = w->black->pixel,
		.colormap = w->cmap,
	    });
    
    w->backbuf = XdbeAllocateBackBufferName(w->dpy, w->win, XdbeUndefined);
    
    XSelectInput(w->dpy, w->win, KeyPressMask | KeyReleaseMask | EnterWindowMask | LeaveWindowMask);
    
    read_images(w);
    
    pthread_create(&w->thread, NULL, screen_x11_thread_out, w);
    pthread_create(&w->thread, NULL, screen_x11_thread_in, w);
    
    return (struct screen_t *) w;
}

static void *screen_x11_thread_out(void *parm)
{
    struct screen_x11_t *w = parm;
    
    XMapWindow(w->dpy, w->win);
    
    GC gc = XCreateGC(w->dpy, w->backbuf, 0, NULL);
    XSetForeground(w->dpy, gc, w->black->pixel);
    XSetFunction(w->dpy, gc, GXcopy);
    XSetGraphicsExposures(w->dpy, gc, False);
    
    while (1) {
	pthread_mutex_lock(&w->event.mutex);
	while (!w->event.ev)
	    pthread_cond_wait(&w->event.cond, &w->event.mutex);
	w->event.ev = 0;
	pthread_mutex_unlock(&w->event.mutex);
	
	XdbeSwapBuffers(w->dpy,
		& (XdbeSwapInfo) {
		    .swap_window = w->win,
		    .swap_action = XdbeUndefined,
		}, 1);
	
	XSetForeground(w->dpy, gc, w->black->pixel);
	XFillRectangle(w->dpy, w->backbuf, gc, 0, 0, 256, 512);
	
	XSetForeground(w->dpy, gc, w->white->pixel);
	for (int y = 2; y <= MAP_HEIGHT; y++)
	    XDrawLine(w->dpy, w->backbuf, gc, 0 * 32, y * 32, MAP_WIDTH * 32, y * 32);
	for (int x = 0; x <= MAP_WIDTH; x++)
	    XDrawLine(w->dpy, w->backbuf, gc, x * 32, 2 * 32, x * 32, MAP_HEIGHT * 32);
	
	for (int y = 0; y < MAP_HEIGHT; y++) {
	    for (int x = 0; x < MAP_WIDTH; x++) {
		switch (w->map.data[y][x].type) {
		case TYPE_NONE:
		    break;
		    
		case TYPE_BALL:
		    XSetForeground(w->dpy, gc, w->blue->pixel);
		    draw_ball(w, w->dpy, w->backbuf, gc,
			    w->map.data[y][x].ball_dir,
			    x, y, 0, 0);
		    break;
		    
		case TYPE_BLOCK:
		    XCopyArea(w->dpy, w->pix.block.pix, w->backbuf, gc,
			    0, 0, 32, 32, x * 32, y * 32);
		    break;
		    
		case TYPE_GANG:
		    draw_gang(w, w->dpy, w->backbuf, gc, x * 32, y * 32, w->map.data[y][x].ctr);
		    break;
		}
	    }
	}
	
	if (w->ball_dir != 0) {
	    XSetForeground(w->dpy, gc, w->blue->pixel);
	    draw_ball(w, w->dpy, w->backbuf, gc, w->ball_dir, w->ball_x, w->ball_y, w->ball_dx, w->ball_dy);
	}
	
	for (int i = 0; i < COUNTOF(w->flick.gangs); i++) {
	    struct flick_gang_t *fgp = &w->flick.gangs[i];
	    if (fgp->used) {
		int x, y;
		x = fgp->x * 32;
		y = fgp->y * 32;
		x += fgp->ctr * 5 * fgp->dir;
		y -= -0.8 * fgp->ctr * fgp->ctr + 8 * fgp->ctr;
		draw_gang(w, w->dpy, w->backbuf, gc, x, y, 0);
	    }
	}
	
	XFlush(w->dpy);
	
	if (w->disp_game_over)
	    printf("game over\n");
    }
    
    return NULL;
}

static void *screen_x11_thread_in(void *parm)
{
    struct screen_x11_t *w = parm;
    
    while (1) {
	XEvent ev;
	XWindowEvent(w->dpy, w->win, ~0, &ev);
	if (ev.type == KeyPress || ev.type == KeyRelease) {
	    int onoff = (ev.type == KeyPress);
	    switch (XKeycodeToKeysym(w->dpy, ev.xkey.keycode, 0)) {
	    case XK_Right:
		w->in.right = onoff;
		break;
	    case XK_Left:
		w->in.left = onoff;
		break;
	    case XK_Up:
		w->in.up = onoff;
		break;
	    case XK_Down:
		w->in.down = onoff;
		break;
	    case XK_z:
		w->in.a = onoff;
		break;
	    case XK_Return:
		w->in.start = onoff;
		break;
	    case XK_p:
		w->in.pause = onoff;
		break;
	    case XK_f:
		w->in.frame = onoff;
		break;
	    }
	} else if (ev.type == EnterNotify) {
	    XAutoRepeatOff(w->dpy);
	} else if (ev.type == LeaveNotify) {
	    XAutoRepeatOn(w->dpy);
	}
    }
}

static void draw_ball(struct screen_x11_t *w, Display *dpy, Drawable win, GC gc,
	int dir, int x, int y, int dx, int dy)
{
    struct chara_t *cp = dir < 0 ? &w->pix.ball_l : &w->pix.ball_r;
    int x0 = x * 32 + dx * 32 / 256;
    int y0 = y * 32 + dy * 32 / 256;
    
    XSetClipMask(w->dpy, gc, cp->mask);
    XSetClipOrigin(w->dpy, gc, x0, y0);
    XCopyArea(w->dpy, cp->pix, w->backbuf, gc, 0, 0, 32, 32, x0, y0);
    XSetClipMask(w->dpy, gc, None);
}

static void draw_gang(struct screen_x11_t *w, Display *dpy, Drawable win, GC gc,
	int x, int y, int ctr)
{
    struct chara_t *cp = &w->pix.gang;
    
    if (ctr != 0) {
	if (ctr % 1000 < 16 * 4 * 2) {
	    switch ((ctr >> 4) & 3) {
	    case 0:
	    case 2:
		cp = &w->pix.gang_d;
		break;
	    case 1:
		cp = &w->pix.gang_l;
		break;
	    case 3:
		cp = &w->pix.gang_r;
	    break;
	    }
	}
    }
    
    XSetClipMask(w->dpy, gc, cp->mask);
    XSetClipOrigin(w->dpy, gc, x, y);
    XCopyArea(w->dpy, cp->pix, w->backbuf, gc, 0, 0, 32, 32, x, y);
    XSetClipMask(w->dpy, gc, None);
}

static void screen_x11_notify(struct screen_t *scr)
{
    struct screen_x11_t *w = (struct screen_x11_t *) scr;
    
    pthread_mutex_lock(&w->event.mutex);
    
    w->event.ev = 1;
    pthread_cond_signal(&w->event.cond);
    
    w->ball_dir = w->frame.ball_dir;
    w->ball_x = w->frame.ball_x;
    w->ball_y = w->frame.ball_y;
    w->ball_dx = w->frame.ball_dx;
    w->ball_dy = w->frame.ball_dy;
    w->frame.ball_dir = 0;
    
    w->map = w->frame.map;
    
    w->disp_game_over = w->frame.disp_game_over;
    w->frame.disp_game_over = 0;
    
    w->flick = w->frame.flick;
    memset(&w->frame.flick, 0, sizeof w->frame.flick);
    
    pthread_mutex_unlock(&w->event.mutex);
}

static void screen_x11_set_ball(struct screen_t *scr, int dir, int x, int y, int dx, int dy)
{
    struct screen_x11_t *w = (struct screen_x11_t *) scr;
    
    w->frame.ball_dir = dir;
    w->frame.ball_x = x;
    w->frame.ball_y = y;
    w->frame.ball_dx = dx;
    w->frame.ball_dy = dy;
}

static int screen_x11_set_flick_gang(struct screen_t *scr,
	int x, int y, int dir, int ctr)
{
    struct screen_x11_t *w = (struct screen_x11_t *) scr;
    
    if (ctr >= 30)
	return 1;
    
    for (int i = 0; i < COUNTOF(w->flick.gangs); i++) {
	if (!w->flick.gangs[i].used) {
	    w->flick.gangs[i].x = x;
	    w->flick.gangs[i].y = y;
	    w->flick.gangs[i].dir = dir;
	    w->flick.gangs[i].ctr = ctr;
	    w->flick.gangs[i].used = 1;
	    return 0;
	}
    }
    
    return 0;
}

static void screen_x11_set_map(struct screen_t *scr, const struct map_t *map)
{
    struct screen_x11_t *w = (struct screen_x11_t *) scr;
    
    w->frame.map = *map;
}

static void screen_x11_set_game_over(struct screen_t *scr)
{
    struct screen_x11_t *w = (struct screen_x11_t *) scr;
    
    w->frame.disp_game_over = 1;
}

static unsigned long screen_x11_in(struct screen_t *scr)
{
    struct screen_x11_t *w = (struct screen_x11_t *) scr;
    
    return w->in.start * SCREEN_IN_START |
	    w->in.a * SCREEN_IN_A |
	    w->in.up * SCREEN_IN_UP |
	    w->in.down * SCREEN_IN_DOWN |
	    w->in.left * SCREEN_IN_LEFT |
	    w->in.right * SCREEN_IN_RIGHT;
}

static int screen_x11_do_frame(struct screen_t *scr)
{
    struct screen_x11_t *w = (struct screen_x11_t *) scr;
    int pause = w->in.pause;
    int frame = w->in.frame;
    int do_frame;
    
    if (!w->pause.last_pause && pause)
	w->pause.pausing = !w->pause.pausing;
    
    do_frame = (!w->pause.pausing || (!w->pause.last_frame && frame));
    
    w->pause.last_pause = pause;
    w->pause.last_frame = frame;
    
    return do_frame;
}

struct screen_ops_t screen_x11_ops = {
    .new = screen_x11_new,
    .notify = screen_x11_notify,
    .set_ball = screen_x11_set_ball,
    .set_flick_gang = screen_x11_set_flick_gang,
    .set_map = screen_x11_set_map,
    .set_game_over = screen_x11_set_game_over,
    .in = screen_x11_in,
    .do_frame = screen_x11_do_frame,
};

static void select_visual(struct screen_x11_t *w)
{
    int event_base, error_base;
    if (!XdbeQueryExtension(w->dpy, &event_base, &error_base)) {
	fprintf(stderr, "double-buffer extension not supported.\n");
	exit(1);
    }
    
    int n = 1;
    XdbeScreenVisualInfo *svi = XdbeGetVisualInfo(w->dpy, &w->root, &n);
    if (svi == NULL) {
	fprintf(stderr, "No suitable visual for double buffer.\n");
	exit(1);
    }
    for (int i = 0; i < svi->count; i++) {
	printf("%08x %d %d\n",
		svi->visinfo[i].visual, svi->visinfo[i].depth, svi->visinfo[i].perflevel);
    }
    
    for (int i = 0; i < w->scr->ndepths; i++) {
	if (w->scr->depths[i].depth == svi->visinfo[0].depth) {
	    for (int j = 0; j < w->scr->depths[i].nvisuals; j++) {
		if (w->scr->depths[i].visuals[j].visualid == svi->visinfo[0].visual) {
		    w->depth = &w->scr->depths[i];
		    w->visual = &w->scr->depths[i].visuals[j];
		}
	    }
	}
    }
    if (w->visual == NULL) {
	fprintf(stderr, "No visual %08x.\n", svi->visinfo[0].visual);
	exit(1);
    }
    
    XdbeFreeVisualInfo(svi);
}

static void read_images(struct screen_x11_t *w)
{
    struct {
	struct chara_t *cp;
	const char *fname;
    } tbl[] = {
	{ &w->pix.block, "block.xpm" },
	{ &w->pix.ball_l, "ball_l.xpm" },
	{ &w->pix.ball_r, "ball_r.xpm" },
	{ &w->pix.gang, "gang.xpm" },
	{ &w->pix.gang_l, "gang_l.xpm" },
	{ &w->pix.gang_r, "gang_r.xpm" },
	{ &w->pix.gang_d, "gang_d.xpm" },
    };
    
    const char *dir = "../image";
    for (int i = 0; i < sizeof tbl / sizeof tbl[0]; i++) {
	int size = strlen(dir) + 1 + strlen(tbl[i].fname) + 1;
	char *path = malloc(size);
	snprintf(path, size, "%s/%s", dir, tbl[i].fname);
	
	XpmAttributes attr;
	memset(&attr, 0, sizeof attr);
	attr.visual = w->visual;
	attr.colormap = w->cmap;
	attr.depth = w->depth->depth;
	attr.valuemask = XpmVisual | XpmColormap | XpmDepth;
	if (XpmReadFileToPixmap(w->dpy, w->win, 
			path, &tbl[i].cp->pix, &tbl[i].cp->mask,
			&attr)) {
	    fprintf(stderr, "%s: error.\n", path);
	    exit(1);
	}
	printf("%s => %08x, %08x\n", path, tbl[i].cp->pix, tbl[i].cp->mask);
	
	free(path);
    }
}
