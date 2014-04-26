#ifndef IN_H__INCLUDED
#define IN_H__INCLUDED

enum {
    IN_UP = 0,
    IN_DOWN,
    IN_LEFT,
    IN_RIGHT,
    IN_A,
    IN_START,
    
    IN_NR
};

enum {
    IN_NOW,
    IN_ON,
    IN_OFF,
    IN_REP,
};

int in_chk(int key, int type);

#endif	/* ifndef IN_H__INCLUDED */
