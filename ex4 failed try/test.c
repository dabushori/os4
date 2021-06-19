#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "osqueue.h"
#include "threadPool.h"


void hello (void* a)
{
   printf("hello\n");
}

void hello2 (void* a)
{
   printf("hello2\n");
}

void hellosleep (void* a) {
   sleep(3);
   printf("sleep for 3 seconds\n");
}

#define SIZE 6

void test_thread_pool_sanity()
{
   int i;
   
   ThreadPool* tp = tpCreate(SIZE);
   
   for(i=0; i<SIZE; ++i)
   {
      if (i % 3 == 0)
         tpInsertTask(tp,hello,NULL);
      else if (i % 3 == 1)
         tpInsertTask(tp,hello2,NULL);
      else 
         tpInsertTask(tp,hellosleep,NULL);
   }
   tpDestroy(tp,1);
}


int main()
{
   test_thread_pool_sanity();

   // pthread_exit((void*)0);
   return 0;
}
