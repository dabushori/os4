// Ori Dabush 212945760

#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include "osqueue.h"
#include <pthread.h>

// a struct to represent a task - a function with an argument
typedef struct Task {
    void (*func)(void*);
    void* arg;
} Task;

// a struct for a thread pool
typedef struct thread_pool {
    // a flag that tells the threads that tpDestory has been called
    int stop; 
    // a flag that tells the threads to finish all the tasks in the queue. if the flag is 0 (and stop is 1), each thread will
    // finish the current task and return, without finishing the tasks that are in the queue.
    int waitForThreads;

    // the threads array
    pthread_t* threads;
    // the number of elements in the array (number of threadsin the thread pool)
    int numOfThreads;

    // a mutex to syncronize everything
    pthread_mutex_t mutex;

    // a condition variable that tells a thread that a new task has been added to the queue
    pthread_cond_t taskAdded;

    // the tasks queue
    OSQueue* taskQueue;

    // the number of threads that are not returned yet.
    int threadsWorking;
    // a condition variable that tells the tpDestory function that all the threads have been returned
    pthread_cond_t threadsDone;
} ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
