// Ori Dabush 212945760

#include <stdio.h> // for debug

#include <stdlib.h>
#include <pthread.h>

#include "osqueue.h"
#include "threadPool.h"

/*
ORIGIN!!!
*/

// a struct for a task, so we can enqueue and dequeue tasks (a function and a parameter)
typedef struct Task {
    void (*computeFunc) (void *);
    void* param;
} Task;

// queue syncronized functions

void sync_enqueue(ThreadPool* pool, Task* task) {
    printf("start %s\n", __func__);
    pthread_mutex_lock(&pool->queueMutex);
    osEnqueue(pool->tasksQueue, (void*) task);
    pthread_mutex_unlock(&pool->queueMutex);
    printf("end %s\n", __func__);
}

Task* sync_dequeue(ThreadPool* pool) {
    printf("start %s\n", __func__);
    Task* res;
    pthread_mutex_lock(&pool->queueMutex);
    res = (Task*) osDequeue(pool->tasksQueue);
    pthread_mutex_unlock(&pool->queueMutex);
    printf("end %s\n", __func__);
    return res;
}

int sync_isEmpty(ThreadPool* pool) {
    printf("start %s\n", __func__);
    int res;
    pthread_mutex_lock(&pool->queueMutex);
    res = osIsQueueEmpty(pool->tasksQueue);
    pthread_mutex_unlock(&pool->queueMutex);
    printf("end %s\n", __func__);
    return res;
}

// accepts tasks flag syncronized functions

int sync_getAcceptsTasks(ThreadPool* pool) {
    printf("start %s\n", __func__);
    int res;
    printf("1\n");
    pthread_mutex_lock(&pool->acceptingMutex);
    printf("2\n");
    res = pool->acceptsTasks;
    printf("3\n");
    pthread_mutex_unlock(&pool->acceptingMutex);
    printf("end %s\n", __func__);
    return res;
}

void sync_setAcceptsTasks(ThreadPool* pool, int newVal) {
    printf("start %s\n", __func__);
    pthread_mutex_lock(&pool->acceptingMutex);
    pool->acceptsTasks = newVal;
    pthread_mutex_unlock(&pool->acceptingMutex);
    printf("end %s\n", __func__);
}

int sync_getQueueAcceptsTasks(ThreadPool* pool) {
    printf("start %s\n", __func__);
    int res;
    pthread_mutex_lock(&pool->acceptingMutex);
    res = pool->queueAcceptsTasks;
    pthread_mutex_unlock(&pool->acceptingMutex);
    printf("end %s\n", __func__);
    return res;
}

void sync_setQueueAcceptsTasks(ThreadPool* pool, int newVal) {
    printf("start %s\n", __func__);
    pthread_mutex_lock(&pool->acceptingMutex);
    pool->queueAcceptsTasks = newVal;
    pthread_mutex_unlock(&pool->acceptingMutex);
    printf("end %s\n", __func__);
}

// thread routine for each thread in the thread pool

void* threadRoutine(void* threadPool) {
    ThreadPool* pool = (ThreadPool*)threadPool;
    while (sync_getAcceptsTasks(threadPool)) {
        if (!sync_isEmpty(threadPool)) {
            // dequeue and execute
            Task* t = sync_dequeue(threadPool);
            void (*computeFunc) (void *) = t->computeFunc;
            void* param = t->param;
            free(t);
            computeFunc(param);
        } else if (sync_getQueueAcceptsTasks(threadPool)) {
            // if there are no tasks, wait for a task to be inserted (no busy waiting!)
            pthread_mutex_lock(&pool->taskAddedMutex);
            pthread_cond_wait(&pool->taskAddedCond,&pool->taskAddedMutex);
            pthread_mutex_unlock(&pool->taskAddedMutex);
        } else {
            // if the thread pool is destroyed and there are no tasks left to execute, return
            return NULL;
        }
    }
    return NULL;
}

ThreadPool* tpCreate(int numOfThreads) {
    // allocate the thread pool
    ThreadPool* tp = malloc(sizeof(ThreadPool));
    if (tp == NULL) {
        // error in malloc
        return NULL;
    }

    tp->acceptsTasks = 1;
    tp->queueAcceptsTasks = 1;

    // initialize the mutex for the acceptingTasks variable
    if (pthread_mutex_init(&tp->acceptingMutex, NULL) != 0) {
        // error in pthread_mutex_init
        free(tp);
        return NULL;
    }
    
    // initialize the queue
    tp->tasksQueue = osCreateQueue();
    if (tp->tasksQueue == NULL) {
        // error in osCreateQueue
        pthread_mutex_destroy(&tp->acceptingMutex);
        free(tp);
        return NULL;
    }
    
    // initialize the condition variable that alerts that a task has been entered
    if(pthread_cond_init(&tp->taskAddedCond, NULL) != 0) {
        // error in pthread_cond_init
        osDestroyQueue(tp->tasksQueue);
        pthread_mutex_destroy(&tp->acceptingMutex);
        free(tp);
        return NULL;
    }

    // initialize the mutex that will be used with the condition variable
    if (pthread_mutex_init(&tp->taskAddedMutex, NULL) != 0) {
        // error in pthread_mutex_init
        pthread_cond_destroy(&tp->taskAddedCond);
        osDestroyQueue(tp->tasksQueue);
        pthread_mutex_destroy(&tp->acceptingMutex);
        free(tp);
        return NULL;
    }

    // initialize the queue mutex
    if (pthread_mutex_init(&tp->queueMutex, NULL) != 0) {
        // error in pthread_mutex_init
        pthread_mutex_destroy(&tp->taskAddedMutex);
        pthread_cond_destroy(&tp->taskAddedCond);
        osDestroyQueue(tp->tasksQueue);
        pthread_mutex_destroy(&tp->acceptingMutex);
        free(tp);
        return NULL;
    }
    
    tp->numOfThreads = numOfThreads;
    
    // allocate the threads
    tp->threads = malloc(sizeof(pthread_t) * numOfThreads);
    if (tp->threads == NULL) {
        // error in malloc
        osDestroyQueue(tp->tasksQueue);
        pthread_cond_destroy(&tp->taskAddedCond);
        pthread_mutex_destroy(&tp->acceptingMutex);
        free(tp);
        return NULL;
    }
    
    // initialize the threads with the thread routine
    for (int i = 0; i < numOfThreads; ++i) {
        if (pthread_create(tp->threads + i, NULL, threadRoutine, tp) != 0) { // to give it the func
            // error in pthread_create
            for (int j = 0; j < i; ++j) {
                pthread_cancel(tp->threads[j]);
            }
            free(tp->threads);
            osDestroyQueue(tp->tasksQueue);
            pthread_cond_destroy(&tp->taskAddedCond);
            pthread_mutex_destroy(&tp->acceptingMutex);
            free(tp);
        return NULL;
        }
    }

    // return a pointer to the new thread pool
    return tp;
}

void freeThreadPoolMemory(ThreadPool* tp) {
    printf("start %s\n", __func__);
    // destory every mutex
    pthread_mutex_destroy(&tp->acceptingMutex);
    pthread_mutex_destroy(&tp->taskAddedMutex);
    pthread_mutex_destroy(&tp->queueMutex);

    // free the threads array
    free(tp->threads);

    // destory the tasks queue
    osDestroyQueue(tp->tasksQueue);

    // free the thread pool
    free(tp);
    printf("end %s\n", __func__);
}

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
    printf("start %s\n", __func__);
    // close the queue for recieving new tasks
    sync_setQueueAcceptsTasks(threadPool, 0); 
    // make the thread not wait to the tasks that are in the queue, if needed
    sync_setAcceptsTasks(threadPool, shouldWaitForTasks); 

    // signal all the blocked threads and destory the condition variable
    pthread_cond_broadcast(&threadPool->taskAddedCond);
    // destory the condition variable
    pthread_cond_destroy(&threadPool->taskAddedCond);

    // wait for all threads
    for (int i = 0; i < threadPool->numOfThreads; ++i) {
        pthread_join(threadPool->threads[i], NULL);
    }

    // free all the memory
    freeThreadPoolMemory(threadPool);
    printf("end %s\n", __func__);
}

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param) {
    printf("start %s\n", __func__);
    // if the queue is closed to tasks (tpDestory has been called already) return -1
    if (!sync_getQueueAcceptsTasks(threadPool)) {
        return -1;
    }

    // allocate a new task and enqueue it
    Task* t = malloc(sizeof(Task));
    if (t == NULL) {
        // error in malloc
        return 1; // error code - to check
    }
    t->computeFunc = computeFunc;
    t->param = param;
    sync_enqueue(threadPool, t);

    // notify one thread that a task has been added
    pthread_mutex_lock(&threadPool->taskAddedMutex);
    pthread_cond_signal(&threadPool->taskAddedCond);
    pthread_mutex_unlock(&threadPool->taskAddedMutex);
    printf("end %s\n", __func__);
    return 0;
}