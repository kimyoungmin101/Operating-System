#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "common_threads.h"

typedef struct _rwlock_t {
    sem_t writelock;
    sem_t mutex;
    sem_t okToRead;
    sem_t okToWrite;
    int AR;
    int AW; 
    int WR; 
    int WW;       // number of Active Readers
} rwlock_t;

void rwlock_init(rwlock_t *rw) {
    rw->AR = 0; rw->AW = 0; rw->WR = 0; rw->WW=0;
    Sem_init(&rw->mutex, 1);
    Sem_init(&rw->okToRead, 0); 
    Sem_init(&rw->okToWrite, 0); 
}

void rwlock_acquire_readlock(rwlock_t *rw) {
    Sem_wait(&rw->mutex);
    while((rw->AW+rw->WW)>0){
        rw->WR++;
        Sem_post(&rw->mutex);
        Sem_wait(&rw->okToRead);
        rw->WR--;
    }
    rw->AR++;
    Sem_post(&rw->mutex);
}

void rwlock_release_readlock(rwlock_t *rw) {
    Sem_wait(&rw->mutex);
    rw->AR--;
    if (rw->AR == 0 && (rw->WW>0))
        Sem_post(&rw->okToWrite);
    Sem_post(&rw->mutex);
}

void rwlock_acquire_writelock(rwlock_t *rw) {
    Sem_wait(&rw->mutex);
    while((rw->AW + rw->AR)>0){
        rw->WW++;
        Sem_post(&rw->mutex);
        Sem_wait(&rw->okToWrite);
        rw->WW--;
    }
    rw->AW++;
    Sem_post(&rw->mutex);
}

void rwlock_release_writelock(rwlock_t *rw) {
    Sem_wait(&rw->mutex);
    rw->AW--;
    if(rw->WW > 0){
        Sem_post(&rw->okToWrite);
    }
    else if(rw->WR > 0){
        sem_post(&rw->okToRead);
        sem_post(&rw->okToRead);
    }
    Sem_post(&rw->mutex);
}


//
// Don't change the code below (just use it!) But fix it if bugs are found!
// 

int loops;
int DB = 0;

typedef struct {
    int thread_id;
    int job_type;         // 0: reader, 1: writer
    int arrival_delay;
    int running_time;
} arg_t;

sem_t print_lock;

#define TAB 10
void space(int s) {
    Sem_wait(&print_lock);
    int i;
    for (i = 0; i < s * TAB; i++)
        printf(" ");
}

void space_end() {
    Sem_post(&print_lock);
}

#define TICK usleep(10000)    // 1/100초 단위로 하고 싶으면 usleep(10000)
rwlock_t rwlock;

    
void *reader(void *arg) {
    arg_t *args = (arg_t *)arg;
    
    TICK;
    rwlock_acquire_readlock(&rwlock);
   // start reading
    int i;
    for (i = 0; i < args->running_time-1; i++) {
        TICK;
        space(args->thread_id); printf("reading %d of %d\n", i, args->running_time); space_end();
    }
    TICK;
    space(args->thread_id); printf("reading %d of %d, DB is %d\n", i, args->running_time, DB); space_end();
    // end reading
    TICK;
    rwlock_release_readlock(&rwlock);
    return NULL;
}

void *writer(void *arg) {
    arg_t *args = (arg_t *)arg;

    TICK;
    rwlock_acquire_writelock(&rwlock);
   // start writing
    int i;
    for (i = 0; i < args->running_time-1; i++) {
        TICK;
        space(args->thread_id); printf("writing %d of %d\n", i, args->running_time); space_end();
    }
    TICK;
    DB++;
    space(args->thread_id); printf("writing %d of %d, DB is %d\n", i, args->running_time, DB); space_end();
    // end writing 
    TICK;
    rwlock_release_writelock(&rwlock);

    return NULL;
}

void *worker(void *arg) {
    arg_t *args = (arg_t *)arg;
    int i;
    for (i = 0; i < args->arrival_delay; i++) {
        TICK;
        space(args->thread_id); printf("arrival delay %d of %d\n", i, args->arrival_delay); space_end();
    }
    if (args->job_type == 0) {
    reader(arg);
    }
    else if (args->job_type == 1) {
    writer(arg);
    }
    else {
        space(args->thread_id); printf("Unknown job %d\n",args->thread_id); space_end();
    }
    return NULL;
}

#define MAX_WORKERS 10

int main(int argc, char *argv[]) {
    
    // command line argument로 공급 받거나  
    // 예: -n 6 -a 0:0:5,0:1:8,1:3:4,0:5:7,1:6:2,0:7:4    또는   -n 6 -a r:0:5,r:1:8,w:3:4,r:5:7,w:6:2,r:7:4
    // 아래 코드에서 for-loop을 풀고 배열 a에 직접 쓰는 방법으로 worker 세트를 구성한다.
    
    int num_workers =6;
    pthread_t p[MAX_WORKERS];
    arg_t a[MAX_WORKERS];
    int num_t[6] = {0,0,1,0,1,0};
    int num_w[6] = {0,1,3,5,6,7};
    int num_c[6] = {5,5,4,3,2,4};
    rwlock_init(&rwlock);
    Sem_init(&print_lock, 1);

    int i;
    for (i = 0; i < num_workers; i++) {
        a[i].thread_id = i;
        a[i].job_type = num_t[i];
        a[i].arrival_delay = num_w[i];
        a[i].running_time = num_c[i];
    }
    
    printf("begin\n");   
    printf(" ... heading  ...  \n");   // a[]의 정보를 반영해서 헤딩 라인을 출력
    
    for (i = 0; i < num_workers; i++)
        Pthread_create(&p[i], NULL, worker, &a[i]);
    
    for (i = 0; i < num_workers; i++)
        Pthread_join(p[i], NULL);

    printf("end: DB %d\n", DB);

    return 0;
}


