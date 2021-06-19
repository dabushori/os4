// Ori Dabush 212945760

#include "threadPool.h"
#include "osqueue.h"

#include <stdlib.h>
#include <stdio.h>

// a function to free all the memory and exit with -1 error code (in case of any error)
void freeAllocatedMemory(ThreadPool* threadPool) {
    pthread_mutex_lock(&threadPool->mutex);
    int i;
    free(threadPool->threads);
    osDestroyQueue(threadPool->taskQueue);
    free(threadPool);
    exit(-1);
}

// the routine of any thread in the thread pool
void* threadRoutine(void* arg) {
    ThreadPool* tp = (ThreadPool*)arg;
    while (1) {
        if (pthread_mutex_lock(&tp->mutex) != 0) {
            perror("Error in pthread_mutex_lock");
            freeAllocatedMemory(tp);
        }

        // if we don't need to stop and the queue is empty, wait for a signal from the condition variable
        if (!tp->stop && osIsQueueEmpty(tp->taskQueue)) {
            pthread_cond_wait(&tp->taskAdded, &tp->mutex);
        }

        // if we need to stop and don't need to wait for the threads in the queue or the queue is empty, break the loop
        if (tp->stop) {
            if (!tp->waitForThreads || osIsQueueEmpty(tp->taskQueue)) {
                if (pthread_mutex_unlock(&tp->mutex) != 0) {
                    perror("Error in pthread_mutex_unlock");
                    freeAllocatedMemory(tp);
                }
                break;    
            }
        }

        // dequeue a task from the queue and execute it
        Task* t = (Task*)osDequeue(tp->taskQueue);
        if (t != NULL) {
            void (*func)(void*) = t->func;
            void* arg = t->arg;
            free(t);

            if (pthread_mutex_unlock(&tp->mutex) != 0) {
                perror("Error in pthread_mutex_unlock");
                freeAllocatedMemory(tp);
            }
            
            func(arg);
        }
    }

    // after we breaked the loop, we decrease the number of working threads by 1, and signaling th econdition variable if all the threads are not working
    // after this part, the current thread done its work
    if (pthread_mutex_lock(&tp->mutex) != 0) {
        perror("Error in pthread_mutex_lock");
        freeAllocatedMemory(tp);
    }
    --(tp->threadsWorking);
    if (tp->threadsWorking == 0) {
        pthread_cond_signal(&tp->threadsDone);
    }
    if (pthread_mutex_unlock(&tp->mutex) != 0) {
        perror("Error in pthread_mutex_unlock");
        freeAllocatedMemory(tp);
    }
    return NULL;
}

// a function that creates a thread poo lwith numOfThreads threads in it
ThreadPool* tpCreate(int numOfThreads) {
    // allocate memory for the thread pool
    ThreadPool* tp = malloc(sizeof(ThreadPool));
    if (tp == NULL) {
        perror("Error in malloc");
        return NULL;
    }

    // initiazlize the values for all the fields (flags, mutex, condition variables and the queue)
    tp->stop = 0;
    tp->waitForThreads = 0;

    if (pthread_mutex_init(&tp->mutex, NULL) != 0) {
        perror("Error in pthread_mutex_init");
        free(tp);
        return NULL;
    }

    if (pthread_cond_init(&tp->taskAdded, NULL) != 0) {
        perror("Error in pthread_cond_init");
        pthread_mutex_destroy(&tp->mutex);
        free(tp);
        return NULL;
    }

    tp->taskQueue = osCreateQueue();
    if (tp->taskQueue == NULL) {
        perror("Error in osCreateQueue");
        pthread_cond_destroy(&tp->taskAdded);
        pthread_mutex_destroy(&tp->mutex);
        free(tp);
        return NULL;
    }

    tp->threadsWorking = numOfThreads;
    if (pthread_cond_init(&tp->threadsDone, NULL) != 0) {
        perror("Error in pthread_cond_init");
        osDestroyQueue(tp->taskQueue);
        pthread_cond_destroy(&tp->taskAdded);
        pthread_mutex_destroy(&tp->mutex);
        free(tp);
        return NULL;
    }

    // alloctaing memory for the threads array and starting the threads with the thread routine
    tp->numOfThreads = numOfThreads;
    tp->threads = malloc(sizeof(pthread_t) * numOfThreads);
    if (tp->threads == NULL) {
        perror("Error in malloc");
        pthread_cond_destroy(&tp->threadsDone);
        osDestroyQueue(tp->taskQueue);
        pthread_cond_destroy(&tp->taskAdded);
        pthread_mutex_destroy(&tp->mutex);
        free(tp);
        return NULL;
    }

    if (pthread_mutex_lock(&tp->mutex) != 0) {
        perror("Error in pthread_mutex_lock");
        free(tp->threads);
        pthread_cond_destroy(&tp->threadsDone);
        osDestroyQueue(tp->taskQueue);
        pthread_cond_destroy(&tp->taskAdded);
        pthread_mutex_destroy(&tp->mutex);
        free(tp);
        return NULL;
    }

    int i;
    for (i = 0; i < numOfThreads; ++i) {
        if (pthread_create(tp->threads + i, NULL, threadRoutine, tp) != 0) {
            perror("Error in pthread_create");
            int j;
            for (j = 0; j < i; ++j) {
                pthread_cancel(tp->threads[j]);
            }
            freeAllocatedMemory(tp);
        }
    }

    if (pthread_mutex_unlock(&tp->mutex) != 0) {
        perror("Error in pthread_mutex_unlock");
        freeAllocatedMemory(tp);
    }
    return tp;
}

// a function to destroy the thread pool
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
    if (pthread_mutex_lock(&threadPool->mutex) != 0) {
        perror("Error in pthread_mutex_lock");
        freeAllocatedMemory(threadPool);
    }

    // if tpDestroy has been already called, return
    if (threadPool->stop == 1) {
        if (pthread_mutex_unlock(&threadPool->mutex) != 0) {
            perror("Error in pthread_mutex_unlock");
            freeAllocatedMemory(threadPool);
        }

        return;
    }

    // turn on the stop flag and turn the waitForThreads flag by the given parameter
    threadPool->stop = 1;
    threadPool->waitForThreads = shouldWaitForTasks;
    // signal all the threads that are waiting in order to finish all the tasks in the queue (if needed) and return
    pthread_cond_broadcast(&threadPool->taskAdded);
    // wait for all the threads to return, the last thread to return will signal this condition variable as we described
    pthread_cond_wait(&threadPool->threadsDone, &threadPool->mutex);

    // join all the threads in order to free their resources
    int i;
    for (i = 0; i < threadPool->numOfThreads; ++i) {
        if(pthread_join(threadPool->threads[i], NULL) != 0) {
            perror("Error in pthread_join");
            freeAllocatedMemory(threadPool);
        }
    }

    // free all the memory
    free(threadPool->threads);
    pthread_cond_destroy(&threadPool->threadsDone);
    osDestroyQueue(threadPool->taskQueue);
    pthread_cond_destroy(&threadPool->taskAdded);
    pthread_mutex_destroy(&threadPool->mutex);
    free(threadPool);
}

// a function to add a task to the thread pool's task queue
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param) {
    if (pthread_mutex_lock(&threadPool->mutex) != 0) {
        perror("Error in pthread_mutex_lock");
        freeAllocatedMemory(threadPool);
    }

    // preventing task inserting in case tpDestory has been already called
    if (threadPool->stop) {
        if (pthread_mutex_unlock(&threadPool->mutex) != 0) {
            perror("Error in pthread_mutex_unlock");
            freeAllocatedMemory(threadPool);
        }
        return -1;
    }

    // allocating memory for the new task and adding it to the queue
    Task* t = malloc(sizeof(Task));
    if (t == NULL) {
        perror("Error in malloc");
        freeAllocatedMemory(threadPool);
    }
    t->func = computeFunc;
    t->arg = param;

    osEnqueue(threadPool->taskQueue, t);

    // signal a thread that a task is available 
    pthread_cond_signal(&threadPool->taskAdded);

    if (pthread_mutex_unlock(&threadPool->mutex) != 0) {
        perror("Error in pthread_mutex_unlock");
        freeAllocatedMemory(threadPool);
    }
    return 0;
}