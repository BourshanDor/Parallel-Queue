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
    atomic_size_t size; // Current size of the queue
    bool empty;
} Queue;

Queue jobs;
Queue workers;

atomic_size_t count_visited = 0;

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

    // Init jobs queue
    jobs.first = NULL;
    jobs.last = NULL;
    jobs.size = 0;
    jobs.empty = true;

    // Init workers queue
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

    // Free jobs queue
    while (!jobs.empty)
    {
        tmp = jobs.first->next;
        free(jobs.first);
        jobs.first = tmp;
        jobs.size--;
        if (jobs.size == 0)
        {
            jobs.empty = true;
        }
    }

    // Free workers queue
    while (!workers.empty)
    {
        tmp = workers.first->next;
        free(workers.first);
        workers.first = tmp;
        workers.size--;
        if (workers.size == 0)
        {
            workers.empty = true;
        }
    }
    count_visited = 0;
    mtx_unlock(&lock);
    mtx_destroy(&lock);
}

void enqueue(void *t)
{
    Node *tmp;
    size_t i;
    mtx_lock(&lock);

    // We do not need to check malloc.
    tmp = malloc(sizeof(Node));
    tmp->data = t;
    tmp->next = NULL;

    if (jobs.empty)
    {
        jobs.first = tmp;
        jobs.last = jobs.first;
        jobs.empty = false;
    }
    else
    {
        jobs.last->next = tmp;
        jobs.last = tmp;
    }
    jobs.size++;

    // Need to send signal to wake up the first worker in the workers queue.
    if (!workers.empty && workers.size >= jobs.size)
    {
        // Need to do a walk to the specific worker.
        tmp = workers.first;
        for (i = 1; i < jobs.size; ++i)
        {
            tmp = tmp->next;
        }
        cnd_signal((cnd_t *)tmp->data);
    }
    // printf("(jobs : %zu, workers :  %zu) \n", jobs.size, workers.size);
    mtx_unlock(&lock);
}

// void enqueue(void *t)
// {
//     Node *tmp;
//     mtx_lock(&lock);

//     // We do not need to check malloc.
//     tmp = malloc(sizeof(Node));
//     tmp->data = t;
//     tmp->next = NULL;

//     if (jobs.empty)
//     {
//         jobs.first = tmp;
//         jobs.last = jobs.first;
//         jobs.empty = false;
//     }
//     else
//     {
//         jobs.last->next = tmp;
//         jobs.last = tmp;
//     }
//     jobs.size++;

//     // Need to send signal to wake up the first worker in the workers queue.
//     if (!workers.empty )
//     {
//         // need to do a walk to the spesific worker.
//         // now the thread on the dequeue can done before enqueue ending safe ?
//         cnd_signal((cnd_t *)workers.first->data);
//     }
//     printf("(jobs : %zu, workers :  %zu) \n", jobs.size, workers.size);
//     mtx_unlock(&lock);
// }

void *dequeue(void)
{
    size_t i;
    cnd_t thread_hold;
    Node *tmp;
    Node *prev;
    void *t;

    mtx_lock(&lock);

    //  ====================================================================================
    // The worker can do his job.
    if (workers.size < jobs.size)
    {
        tmp = jobs.first;
        prev = NULL;
        for (i = 1; i <= workers.size; ++i)
        {
            prev = tmp;
            tmp = tmp->next;
        }

        t = tmp->data;
        if (jobs.size == 1)
        {
            jobs.first = NULL;
            jobs.last = NULL;
        }
        else
        {
            if (tmp->next == NULL)
            {
                jobs.last = prev;
                jobs.last->next = NULL;
            }
            else
            //
            {
                if (prev == NULL)
                {
                    jobs.first = tmp->next;
                }
                else
                {
                    prev->next = tmp->next;
                }
            }
        }

        jobs.size--;
        if (jobs.size == 0)
        {
            jobs.empty = true;
        }

        free(tmp);
        mtx_unlock(&lock);
        return t;
    }

    else
    {

        tmp = malloc(sizeof(Node));
        tmp->data = &thread_hold;
        tmp->next = NULL;
        if (!workers.empty || jobs.empty)
        {
            // jobs.empty
            if (workers.empty)
            {
                workers.first = tmp;
                workers.last = tmp;
                workers.empty = false;
            }
            else
            {
                workers.last->next = tmp;
                workers.last = tmp;
            }

            workers.size++;
            cnd_init(&thread_hold);
            cnd_wait(&thread_hold, &lock);

            tmp = workers.first;
            workers.first = workers.first->next;
            workers.size--;
            if (workers.size == 0)
            {
                workers.empty = true;
            }
            free(tmp);
        }

        /*
        If the 'if boolean' is false it is mean: "workers.empty && !jobs.empty  == true"
        otherwise we enter the if and now we deal with one job at least in jobs queue,
        because in enqueue we send a signal to the thread/worker ask for
        a job, and we notify him that jobs has arrive.
        We have one worker at least that can do the job(the one that call to the function).
        With confident we can return the first job in the jobs queue.
        */

        tmp = jobs.first;
        jobs.first = jobs.first->next;
        jobs.size--;
        if (jobs.size == 0)
        {
            jobs.empty = true;
        }

        t = tmp->data;
    }

    free(tmp);
    //  =====================================================================================

    mtx_unlock(&lock);
    count_visited++;
    return t;
}

bool tryDequeue(void **t)
{
    size_t i;
    Node *tmp;
    Node *prev;

    mtx_lock(&lock);

    if (jobs.empty)
    {
        mtx_unlock(&lock);
        return false;
    }

    if (workers.size < jobs.size)
    {
        tmp = jobs.first;
        prev = NULL;
        for (i = 1; i <= workers.size; ++i)
        {
            prev = tmp;
            tmp = tmp->next;
        }

        *t = tmp->data;
        if (jobs.size == 1)
        {
            jobs.first = NULL;
            jobs.last = NULL;
        }
        else
        {
            if (tmp->next == NULL)
            {
                jobs.last = prev;
                jobs.last->next = NULL;
            }
            else
            {

                if (prev == NULL)
                {
                    jobs.first = tmp->next;
                }
                else
                {
                    prev->next = tmp->next;
                }
            }
        }

        jobs.size--;
        if (jobs.size == 0)
        {
            jobs.empty = true;
        }

        free(tmp);
        mtx_unlock(&lock);
        count_visited++;
        return true;
    }

    return false;
}

size_t size(void)
{
    return jobs.size;
}
size_t waiting(void)
{

    return workers.size;
}
size_t visited(void)
{
    return count_visited;
}
