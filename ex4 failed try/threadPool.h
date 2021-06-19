// Ori Dabush 212945760

#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include "osqueue.h"

// a struct for a thread pool
typedef struct thread_pool {
    // flag that tells if the threads should execute tasks from the queue
    int acceptsTasks; 
    // flag that tells if the queue should accept tasks
    int queueAcceptsTasks; 
    // a mutex for both of the flags
    pthread_mutex_t acceptingMutex; 

    // a condition variable that alerts when a task is added
    pthread_cond_t taskAddedCond;
    // a mutex that will be used with the condition variable
    pthread_mutex_t taskAddedMutex;

    // the number of threads in the thread pool
    int numOfThreads;
    // the threads array
    pthread_t* threads;

    // a queue that contains the tasks that need to be executed
    OSQueue* tasksQueue;
    // a mutex that will be used to syncronize queue operations
    pthread_mutex_t queueMutex;
} ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
