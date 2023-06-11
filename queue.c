#include "queue.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <threads.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Node
{
    void *data;        // Pointer to the data stored in the node
    struct Node *next; // Pointer to the next node in the linked list
} Node;

typedef struct
{
    Node *first;        // Pointer of the first element that arrive
    Node *last;         // Pointer of the last element that arrive
    size_t size;        // Current size of the queue
    bool empty;
} Queue;

Queue jobs;
Queue workers;

atomic_int count_visited = 0;

mtx_t lock;

/*
Need to check if I need to initialized the lock outside the function.
The reason for that it is that the user can creat a new thread and call to
enqueue (for example), the CPU will gave to that thread before we initialized the lock,
in it turn, ask the lock, therefore, an unexpected behaver could be occurs.
*/
void initQueue(void)
{
    int rc;
    rc = mtx_init(&lock, mtx_plain);
    if (rc != thrd_success)
    {
        printf("ERROR in mtx_init()\n");
        exit(-1);
    }
    mtx_lock(&lock);

    // init jobs queue
    jobs.first = NULL;
    jobs.last = NULL;
    jobs.size = 0;
    jobs.empty = true;

    // init workers queue
    workers.first = NULL;
    workers.last = NULL;
    workers.size = 0;
    workers.empty = true;

    mtx_unlock(&lock);
}

void destroyQueue(void)
{
    Node *tmp;
    mtx_lock(&lock);
    
    // free jobs queue 
    while (!jobs.empty)
    {
        tmp = jobs.first->next;
        free(jobs.first)
            jobs.first = tmp;
        jobs.size --;
        if (jobs.size == 0)
        {
            jobs.empty = true;
        }
    }

    // free workers queue 
    while (!workers.empty)
    {
        tmp = workers.first->next;
        free(workers.first)
        workers.first = tmp;
        workers.size --;
        if (workers.size == 0)
        {
            workers.empty = true;
        }
    }
    mtx_unlock(&lock);
    mtx_destroy(&lock);
}


void enqueue(void *t)
{
    Node *tmp;
    mtx_lock(&lock);

    //We do not need to check malloc. 
    tmp = malloc(sizeof(Node));
    tmp->data = t;
    tmp->next = NULL; 

    if (jobs.empty){
        jobs.first = tmp;  
        jobs.last = jobs.first;
        jobs.empty = false; 

    }
    else { 
        jobs.last->next = tmp;
        jobs.last =  tmp; 
    }
    jobs.size ++; 

    if (!workers.empty){
        // need to get wake up the first in the queue. 
    }

    mtx_unlock(&lock);

}


void *dequeue(void)
{


    // could not back
    return NULL;
}
bool tryDequeue(void **x)
{
    return 0;
}
size_t size(void)
{
    return 0;
}
size_t waiting(void)
{
    return 0;
}
size_t visited(void)
{
    return 0;
}


