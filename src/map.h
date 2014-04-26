#ifndef MAP_H__INCLUDED
#define MAP_H__INCLUDED

#define MAP_WIDTH 6
#define MAP_HEIGHT (13 + 2)

enum {
    TYPE_NONE,
    TYPE_BLOCK,
    TYPE_GANG,
    TYPE_BALL,
};

struct map_data_t {
    int type;
    int ball_dir;
    int ctr;
};

struct map_t {
    struct map_data_t data[MAP_HEIGHT][MAP_WIDTH];
};

#endif	/* ifndef MAP_H__INCLUDED */
