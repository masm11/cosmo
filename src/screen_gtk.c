#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include "screen.h"

struct screen_gtk_t {
    const struct screen_ops_t *ops;
    
    pthread_t thread;
    
    struct {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int ev;
    } event;
};

static void *screen_gtk_thread(void *parm);

static struct screen_t *screen_gtk_new(int *argcp, char **argv)
{
    struct screen_gtk_t *w = malloc(sizeof *w);
    
    if (!gtk_init_check(argcp, &argv))
	return NULL;
    
    memset(w, 0, sizeof *w);
    
    w->ops = &screen_gtk_ops;
    
    pthread_mutex_init(&w->event.mutex, NULL);
    pthread_cond_init(&w->event.cond, NULL);
    
    pthread_create(&w->thread, NULL, screen_gtk_thread, w);
    
    return (struct screen_t *) w;
}

static void *screen_gtk_thread(void *parm)
{
    struct screen_gtk_t *w = parm;
    GtkWidget *top;
    
    top = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    
    
    
    gtk_widget_show_all(top);
    
    while (1) {
	pthread_mutex_lock(&w->event.mutex);
	while (!w->event.ev)
	    pthread_cond_wait(&w->event.cond, &w->event.mutex);
	w->event.ev = 0;
	pthread_mutex_unlock(&w->event.mutex);
	
	printf("screen update.\n");
    }
    
    return NULL;
}

static void screen_gtk_notify_update(struct screen_t *scr)
{
    struct screen_gtk_t *w = (struct screen_gtk_t *) scr;
    
    pthread_mutex_lock(&w->event.mutex);
    w->event.ev = 1;
    pthread_cond_signal(&w->event.cond);
    pthread_mutex_unlock(&w->event.mutex);
}

struct screen_ops_t screen_gtk_ops = {
    .new = screen_gtk_new,
    .notify_update = screen_gtk_notify_update,
};
