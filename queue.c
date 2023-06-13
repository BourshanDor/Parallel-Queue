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
    void *data;         // Pointer to the data stored in the node
    cnd_t *thread_hold; // Pointer to the thread_hold stored in the node
    struct Node *next;  // Pointer to the next node in the linked list

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

void enterJobToJobsQueue(void *t);
int wakeUpWorker(void);
void *waitForAJob(void);
void *getAJob(void);
void destroyWorkers(void);
void destroyJobs(void);
void initWorkers(void);
void initJobs(void);
void removeJob(void *node);
void removeWorker(void *node);
void enterWorker(void *node);
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

    // Init jobs and Workers queue
    initJobs();
    initWorkers();

    mtx_unlock(&lock);
}

void destroyQueue(void)
{

    mtx_lock(&lock);

    // Free jobs and workers queue
    destroyJobs();
    destroyWorkers();

    count_visited = 0;
    mtx_unlock(&lock);
    mtx_destroy(&lock);
}

void enqueue(void *t)
{

    mtx_lock(&lock);

    enterJobToJobsQueue(t);

    // Need to send signal to wake up worker in the workers queue.
    if (!workers.empty && workers.size >= jobs.size)
    {
        if (wakeUpWorker() != 0)
        {
            printf("an error have been occurs sending a signal.\n");
        }
    }

    mtx_unlock(&lock);
}

void *dequeue(void)
{
    void *t;

    mtx_lock(&lock);
    if (workers.size < jobs.size)
    {
        t = getAJob();
    }

    else
    {
        t = waitForAJob();
    }

    count_visited++;
    mtx_unlock(&lock);

    return t;
}

bool tryDequeue(void **t)
{

    mtx_lock(&lock);

    if (jobs.empty)
    {
        mtx_unlock(&lock);
        return false;
    }

    if (workers.size < jobs.size)
    {
        // Need to check
        *t = getAJob();

        mtx_unlock(&lock);
        count_visited++;
        return true;
    }

    mtx_unlock(&lock);
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

void initJobs(void)
{
    jobs.first = NULL;
    jobs.last = NULL;
    jobs.size = 0;
    jobs.empty = true;
}

void initWorkers(void)
{
    workers.first = NULL;
    workers.last = NULL;
    workers.size = 0;
    workers.empty = true;
}

void destroyJobs(void)
{

    Node *tmp;
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
}

void destroyWorkers(void)
{

    Node *tmp;
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
}

/*
There is more jobs then workers,
To workers have demand (and we are in happy times).
*/
void *getAJob(void)
{
    size_t i;
    Node *tmp;
    void *t;

    tmp = jobs.first;
    for (i = 1; i <= workers.size; ++i)
    {
        tmp = tmp->next;
    }

    t = tmp->data;
    removeJob(tmp);
    free(tmp);
    return t;
}

/*
There is more workers then jobs,
We are living at awful times.
*/
void *waitForAJob(void)
{

    cnd_t thread_hold;
    cnd_t *job_thread_hold;
    Node *tmp;
    void *t;

    tmp = malloc(sizeof(Node));
    tmp->next = NULL;
    tmp->data = NULL;

    enterWorker(tmp);

    cnd_init(&thread_hold);
    tmp->thread_hold = &thread_hold;
    cnd_wait(&thread_hold, &lock);

    // It is time to get a job.
    // First remove the worker from the queue.

    removeWorker(tmp);

    job_thread_hold = tmp->thread_hold;

    free(tmp);

    tmp = jobs.first;

    while (tmp->thread_hold != job_thread_hold)
    {
        tmp = tmp->next;
    }
    // Then remove the job from the queue.
    removeJob(tmp);

    t = tmp->data;

    free(tmp);
    return t;
}

/*
There is more workers then jobs, so the last jobs enter can be done.
*/
int wakeUpWorker(void)
{
    Node *tmp;
    size_t i;
    int rc;

    tmp = workers.first;
    for (i = 1; i < jobs.size; ++i)
    {
        tmp = tmp->next;
    }

    rc = cnd_signal((cnd_t *)tmp->thread_hold);
    if (thrd_success == rc)
    {
        // probelm with the
        jobs.last->thread_hold = tmp->thread_hold;
        return 0;
    }

    return 1;
}

void enterJobToJobsQueue(void *t)
{
    Node *tmp;
    tmp = malloc(sizeof(Node));
    tmp->data = t;
    tmp->next = NULL;
    tmp->thread_hold = NULL;

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
}

void removeJob(void *node)
{
    size_t i;
    Node *tmp;
    Node *prev = NULL;

    tmp = jobs.first;
    for (i = 1; i < jobs.size; ++i)
    {
        if (tmp == node)
        {
            break;
        }

        prev = tmp;
        tmp = tmp->next;
    }

    if (prev == NULL)
    {
        if (jobs.size == 1)
        {
            jobs.first = NULL;
            jobs.first = NULL;
        }
        else
        {
            jobs.first = jobs.first->next;
        }
    }
    else
    {
        if (tmp == jobs.last)
        {
            jobs.last = prev;
        }
        prev->next = tmp->next;
    }

    jobs.size--;
    if (jobs.size == 0)
    {
        jobs.empty = true;
    }
}

void removeWorker(void *node)
{
    size_t i;
    Node *tmp;
    Node *prev = NULL;

    tmp = workers.first;
    for (i = 1; i < workers.size; ++i)
    {

        if (tmp == node)
        {
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }

    if (prev == NULL)
    {
        if (workers.size == 1)
        {
            workers.first = NULL;
            workers.first = NULL;
        }
        else
        {
            workers.first = workers.first->next;
        }
    }
    else
    {
        if (tmp == workers.last)
        {
            workers.last = prev;
        }
        prev->next = tmp->next;
    }

    workers.size--;
    if (workers.size == 0)
    {
        workers.empty = true;
    }
}

void enterWorker(void *node)
{
    if (workers.empty)
    {
        workers.first = node;
        workers.last = node;
        workers.empty = false;
    }

    else
    {
        workers.last->next = node;
        workers.last = node;
    }
    workers.size++;
}
