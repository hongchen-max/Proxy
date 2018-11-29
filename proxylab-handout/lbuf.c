/* $begin lbufc */
#include "csapp.h"
#include "lbuf.h"

/* Create an empty, bounded, shared FIFO buffer with n slots */
/* $begin lbuf_init */
void lbuf_init(lbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(char*));
    for(int i = 0; i < n; i++){
        sp->buf[i] = Calloc(MAXLINE, sizeof(char));
    }

    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
}
/* $end lbuf_init */

/* Clean up buffer sp */
/* $begin lbuf_deinit */
void lbuf_deinit(lbuf_t *sp)
{
    for(int i = 0; i < sp->n; i++){
        Free(sp->buf[i]);
    }
    Free(sp->buf);
}
/* $end lbuf_deinit */

/* Insert item onto the rear of shared buffer sp */
/* $begin lbuf_insert */
void lbuf_insert(lbuf_t *sp, char* item)
{
    P(&sp->slots);                          /* Wait for available slot */
    P(&sp->mutex);                          /* Lock the buffer */
    memset(sp->buf[(++sp->rear)%(sp->n)], 0, MAXLINE);
    strcpy(sp->buf[(sp->rear)%(sp->n)], item); /* Insert the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->items);                          /* Announce available item */
}
/* $end lbuf_insert */

/* Remove and return the first item from buffer sp */
/* $begin lbuf_remove */
char* lbuf_remove(lbuf_t *sp)
{
    char* item = Calloc(MAXLINE, sizeof(char)); 
    
	P(&sp->items);                          /* Wait for available item */
    P(&sp->mutex);                          /* Lock the buffer */
    strcpy(item, sp->buf[(++sp->front)%(sp->n)]);  /* Remove the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->slots);                          /* Announce available slot */
    return item;
}
/* $end lbuf_remove */
/* $end lbufc */
