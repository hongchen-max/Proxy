#ifndef __LBUF_H__
#define __LBUF_H__

#include "csapp.h"

/* $begin lbuft */
typedef struct {
    char **buf;          /* Buffer array */
    int n;             /* Maximum number of slots */
    int front;         /* buf[(front+1)%n] is first item */
    int rear;          /* buf[rear%n] is last item */
    sem_t mutex;       /* Protects accesses to buf */
    sem_t slots;       /* Counts available slots */
    sem_t items;       /* Counts available items */
} lbuf_t;
/* $end lbuft */

void lbuf_init(lbuf_t *sp, int n);
void lbuf_deinit(lbuf_t *sp);
void lbuf_insert(lbuf_t *sp, char* item);
char* lbuf_remove(lbuf_t *sp);

#endif /* __LBUF_H__ */
