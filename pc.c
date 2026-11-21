#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
struct node{
    int number;
    struct node*next;
};
sem_t psem;
sem_t csem;
//互斥锁
pthread_mutex_t mutex;
struct node*head=NULL;
void*producer(void*arg){
    while(true){
        sem_wait(&psem);
        pthread_mutex_lock(&mutex);
        struct node*cur=(struct node*)malloc(sizeof( struct node));
        cur->number=rand()%1000;
        cur->next=head;
        head=cur;
         printf("+++producer, number = %d, tid = %ld\n", cur->number, pthread_self());
        pthread_mutex_unlock(&mutex);
        sem_post(&csem);
        sleep(rand()%3);
    }
    return NULL;
}
void*consumer(void*arg){
    while(true){
        sem_wait(&csem);
        pthread_mutex_lock(&mutex);
    struct node*cur=head;
     printf("+++consumer, number = %d, tid = %ld\n", cur->number, pthread_self());
     head=head->next;
     free(cur);
     pthread_mutex_unlock(&mutex);
     sem_post(&psem);
     sleep(rand()%3);
    }
    return NULL;
}
int main(){
    srand(time(NULL));
sem_init(&psem,0,5);
sem_init(&csem,0,0);
pthread_mutex_init(&mutex,NULL);
pthread_t ptid[5];
pthread_t stid[5];
for(int i=0;i<5;i++){
pthread_create(&ptid[i],NULL,producer,NULL);
}
for(int i=0;i<5;i++){
pthread_create(&stid[i],NULL,consumer,NULL);
}
for(int i=0;i<5;i++){
    pthread_join(ptid[i],NULL);
}
for(int i=0;i<5;i++){
    pthread_join(stid[i],NULL);
}
sem_destroy(&psem);
sem_destroy(&csem);
pthread_mutex_destroy(&mutex);
return 0;
}