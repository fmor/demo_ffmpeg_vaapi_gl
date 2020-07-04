#include "queue.h"
#include <assert.h>

Queue* queue_alloc(uint max, Queue_free_function free_func )
{
    int r;
    Queue* q;
    q = (Queue*) calloc( 1, sizeof(Queue) );
    if( q == NULL )
        goto LBL_FAILED;

    r = pthread_mutex_init( &q->mutex, NULL );
    if( r != 0 )
        goto LBL_FAILED;
    r = pthread_cond_init( &q->cond_not_full, NULL );
    if( r != 0 )
        goto LBL_FAILED;
    r = pthread_cond_init( &q->cond_not_empty, NULL );
    if( r != 0 )
        goto LBL_FAILED;
    q->first = NULL;
    q->last = NULL;
    q->count = 0;
    q->max = max;
    q->free_func = free_func;
    return q;

LBL_FAILED:
    if( q )
    {
        pthread_mutex_destroy( &q->mutex );
        pthread_cond_destroy( &q->cond_not_full );
        pthread_cond_destroy( &q->cond_not_empty );
        free(q);
    }
    return NULL;
}

void queue_free(Queue** q)
{
    queue_flush( *q );
    pthread_mutex_destroy( &(*q)->mutex );
    pthread_cond_destroy( &(*q)->cond_not_full );
    pthread_cond_destroy( &(*q)->cond_not_empty );
    free(*q);
    *q = NULL;

}

void queue_flush(Queue* q)
{
    Queue_element* e;
    Queue_element* n;
    n = NULL;

    pthread_mutex_lock( &q->mutex );
    e = q->first;

    while( e )
    {
        if( q->free_func )
            q->free_func( e->data );
        n = e->next;
        free( e );
        e = n;
    }
    q->first = NULL;
    q->last = NULL;
    q->count = 0;
    pthread_cond_signal( &q->cond_not_full );
    pthread_mutex_unlock( &q->mutex );
}

void queue_push(Queue* q, void* data)
{
    Queue_element* e;
    e =(Queue_element*) malloc( sizeof(Queue_element) );
    if( e == NULL )
        goto LBL_FAILED;
    e->data = data;
    e->next = NULL;
    pthread_mutex_lock( &q->mutex );
    if( q->max )
    {
        while( q->count == (q->max-1) )
            pthread_cond_wait( &q->cond_not_full, &q->mutex );
    }
    if( q->first == NULL )
        q->first = e;
    else
        q->last->next = e;
    q->last = e;
    ++q->count;
    pthread_cond_signal( &q->cond_not_empty );
    pthread_mutex_unlock( &q->mutex);
    return;
LBL_FAILED:
    assert( 0 && "malloc failed");
}

void* queue_pop(Queue* q)
{
    void* data;
    Queue_element* p;
    pthread_mutex_lock( &q->mutex );
    while( q->count == 0 )
        pthread_cond_wait( &q->cond_not_empty, &q->mutex );

    p = q->first;
    data = q->first->data;
    q->first = q->first->next;
    if( q->first == NULL )
        q->last = NULL;
    --q->count;
    pthread_cond_signal( &q->cond_not_full );
    pthread_mutex_unlock( &q->mutex);
    free(p);
    return data;
}
