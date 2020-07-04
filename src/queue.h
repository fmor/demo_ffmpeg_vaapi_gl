#ifndef INCLUDE_QUEUE
#define INCLUDE_QUEUE

#include <libavcodec/avcodec.h>
#include <pthread.h>

typedef void(*Queue_free_function)(void*);


typedef struct Queue_element
{
    void* data;
    struct Queue_element* next;
} Queue_element;

typedef struct
{
    Queue_element*          first;
    Queue_element*          last;
    pthread_cond_t          cond_not_empty;
    pthread_cond_t          cond_not_full;
    pthread_mutex_t         mutex;
    uint                    count;
    uint                    max;
    Queue_free_function     free_func;
} Queue;


Queue* queue_alloc( uint max, Queue_free_function free_fun );
void queue_free( Queue** q  );
void queue_flush( Queue* q );

void queue_push( Queue* q, void* data );
void* queue_pop( Queue* q );





#endif


