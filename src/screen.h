#ifndef SCREEN_H__INCLUDED
#define SCREEN_H__INCLUDED

#define SCREEN_IN_UP	(1 << 0)
#define SCREEN_IN_DOWN	(1 << 1)
#define SCREEN_IN_LEFT	(1 << 2)
#define SCREEN_IN_RIGHT	(1 << 3)
#define SCREEN_IN_A	(1 << 4)
#define SCREEN_IN_START	(1 << 5)

struct screen_t {
    const struct screen_ops_t *ops;
};

struct map_t;

struct screen_ops_t {
    struct screen_t *(*new)(int *, char **);
    void (*notify)(struct screen_t *);
    void (*set_ball)(struct screen_t *scr, int dir, int x, int y, int dx, int dy);
    int (*set_flick_gang)(struct screen_t *, int x, int y, int dir, int ctr);
    void (*set_map)(struct screen_t *, const struct map_t *);
    void (*set_game_over)(struct screen_t *);
    unsigned long (*in)(struct screen_t *);
    int (*do_frame)(struct screen_t *);
};

extern struct screen_ops_t screen_gtk_ops;
extern struct screen_ops_t screen_x11_ops;
extern struct screen_ops_t screen_tty_ops;

static inline void screen_notify(struct screen_t *scr)
{
    (*scr->ops->notify)(scr);
}

static inline void screen_set_ball(struct screen_t *scr, int dir, int x, int y, int dx, int dy)
{
    (*scr->ops->set_ball)(scr, dir, x, y, dx, dy);
}

static inline int screen_set_flick_gang(struct screen_t *scr,
	int x, int y, int dir, int ctr)
{
    return (*scr->ops->set_flick_gang)(scr, x, y, dir, ctr);
}

static inline void screen_set_map(struct screen_t *scr, const struct map_t *map)
{
    (*scr->ops->set_map)(scr, map);
}

static inline void screen_set_game_over(struct screen_t *scr)
{
    (*scr->ops->set_game_over)(scr);
}

static inline unsigned long screen_in(struct screen_t *scr)
{
    return (*scr->ops->in)(scr);
}

static inline int screen_do_frame(struct screen_t *scr)
{
    return (*scr->ops->do_frame)(scr);
}

#endif	/* ifndef SCREEN_H__INCLUDED */
