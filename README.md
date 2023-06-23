# Parallel Queue
This repository contains an implementation of a generic concurrent FIFO (First-In-First-Out) queue that supports enqueue and dequeue operations using threads. (Assignment 4, Operating Systems course at Tel Aviv University) 

## Introduction
The goal of this project is to provide hands-on experience with threads and concurrent programming. The queue implementation allows multiple threads to enqueue and dequeue items concurrently while maintaining thread safety and preserving the FIFO order.

## Usage
The following functions are available in the queue library:

* ```void initQueue(void);```: Initializes the queue before it is used.
* ```void destroyQueue(void);```: Cleans up the queue when it is no longer needed.
* ```void enqueue(void*);```: Adds an item to the queue.
* ```void* dequeue(void);```: Removes and returns an item from the queue. Blocks if the queue is empty.
* ```bool tryDequeue(void**);```: Attempts to remove an item from the queue. If successful, returns the item and true. If the queue is empty, returns false.
* ```size_t size(void);```: Returns the current number of items in the queue.
* ```size_t waiting(void);```: Returns the number of threads waiting for the queue to fill.
* ```size_t visited(void);```: Returns the number of items that have been inserted and removed from the queue.
