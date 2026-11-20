#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#define NUMBER 2
//任务结构体
typedef struct task{
    void(*function)(void*arg);
    void*arg;
}task;
//线程池结构体
struct threadpool{
    task*taskq;//任务队列
    int queuecapacity;//任务队列容量
    int queuesize;//任务队列当前任务数
    int queuefront;
    int queuerear;
    pthread_t managerid;
    pthread_t*threadids;//工作线程id
    int minnum;
    int maxnum;
    int busynum;//忙线程数
    int livenum;//存活线程数
    int exitnum;//要销毁的线程数
    pthread_mutex_t mutexpool;//锁整个线程池
    pthread_mutex_t mutexbusy;//锁忙线程数
    pthread_cond_t notfull;//任务队列不满
    pthread_cond_t notempty;//任务队列不空
   int shutdown;//线程池是否关闭
};
typedef struct threadpool threadpool;
// 前置声明
void* worker(void* arg);
void* manager(void* arg);
void threadexit(threadpool* pool);
threadpool*threadpoolcreate(int min,int max,int queuesize){
    threadpool*pool=(threadpool*)malloc(sizeof(threadpool));
    do{
        if(pool==NULL){
            printf("malloc threadpool fail\n");
            break;
        }
        pool->threadids = malloc(sizeof(pthread_t)*max);
        memset(pool->threadids,0,sizeof(pthread_t)*max);
        pool->minnum=min;//最小线程数
        pool->maxnum=max;//最大线程数
        pool->busynum=0;
        pool->livenum=min;
        pool->exitnum=0;
        if(pthread_mutex_init(&pool->mutexpool,NULL)!=0||
           pthread_mutex_init(&pool->mutexbusy,NULL)!=0||
           pthread_cond_init(&pool->notfull,NULL)!=0||
           pthread_cond_init(&pool->notempty,NULL)!=0){
            printf("init mutex or cond fail\n");
            break;
        }
        pool->taskq=(task*)malloc(sizeof(task)*queuesize);//任务队列开辟空间
        pool->queuecapacity=queuesize;
        pool->queuesize=0;
        pool->queuefront=0;
        pool->queuerear=0;
        pool->shutdown=0;
        pthread_create(&pool->managerid,NULL,manager,pool);
     for(int i=0;i<min;i++){
            pthread_create(&pool->threadids[i],NULL,worker,pool);
        }
        return pool;
     }while(0);
     //释放资源
     if(pool&&pool->taskq){
        free(pool->taskq);
    }
    if(pool&&pool->threadids){
        free(pool->threadids);
    }
    if(pool){
        free(pool);
    }
    return NULL;
}
int threadpooldestroy(threadpool*pool){
    if(pool==NULL){
        return -1;
    }
    pool->shutdown=1;
    pthread_join(pool->managerid,NULL);
    for(int i=0;i<pool->livenum;i++){
        pthread_cond_signal(&pool->notempty);
    }
    if(pool->taskq){
        free(pool->taskq);
    }
    if(pool->threadids){
        free(pool->threadids);
    }
    pthread_mutex_destroy(&pool->mutexpool);
    pthread_mutex_destroy(&pool->mutexbusy);
    pthread_cond_destroy(&pool->notfull);
    pthread_cond_destroy(&pool->notempty);
    free(pool);
    pool=NULL;
    return 0;
}
int threadpooladd(threadpool*pool,void(*func)(void*arg),void*arg){//向任务队列中添加任务
    pthread_mutex_lock(&pool->mutexpool);
    while(pool->queuesize==pool->queuecapacity&&!pool->shutdown){
        pthread_cond_wait(&pool->notfull,&pool->mutexpool);
    }
    if(pool->shutdown){
        pthread_mutex_unlock(&pool->mutexpool);
        return -1;
    }
    pool->taskq[pool->queuerear].function=func;
    pool->taskq[pool->queuerear].arg=arg;
    pool->queuerear=(pool->queuerear+1)%pool->queuecapacity;
    pool->queuesize++;
    pthread_cond_signal(&pool->notempty);
    pthread_mutex_unlock(&pool->mutexpool);
    return 0;
}
int threadpoolbusy(threadpool*pool){
    pthread_mutex_lock(&pool->mutexpool);
    int busy=pool->busynum;
    pthread_mutex_unlock(&pool->mutexpool);
    return busy;
}
int threadpoolalive(threadpool*pool){
    pthread_mutex_lock(&pool->mutexpool);
    int alive=pool->livenum;
    pthread_mutex_unlock(&pool->mutexpool);
    return alive;
}
void*worker(void*arg){//工作线程
    threadpool*pool=(threadpool*)arg;
    while(1){
        pthread_mutex_lock(&pool->mutexpool);
        while(pool->queuesize==0&&!pool->shutdown){
            pthread_cond_wait(&pool->notempty,&pool->mutexpool);
            if(pool->exitnum>0){
                pool->exitnum--;
                if(pool->livenum>pool->minnum){
                    pool->livenum--;
                    pthread_mutex_unlock(&pool->mutexpool);
                    threadexit(pool);
                }
            }
        }
        if(pool->shutdown){
            pthread_mutex_unlock(&pool->mutexpool);
            threadexit(pool);
        }
        //取出任务
        task task;
        task.function=pool->taskq[pool->queuefront].function;
        task.arg=pool->taskq[pool->queuefront].arg;
        pool->queuefront=(pool->queuefront+1)%pool->queuecapacity;
        pool->queuesize--;
        pthread_cond_signal(&pool->notfull);
        pthread_mutex_unlock(&pool->mutexpool);
        printf("thread %ld start working\n",pthread_self());
        pthread_mutex_lock(&pool->mutexbusy);
        pool->busynum++;
        pthread_mutex_unlock(&pool->mutexbusy);
        task.function(task.arg);
        free(task.arg);
        task.arg=NULL;
        printf("thread %ld end working\n",pthread_self());
        pthread_mutex_lock(&pool->mutexbusy);
        pool->busynum--;
        pthread_mutex_unlock(&pool->mutexbusy);
        }
        return NULL;
}
void*manager(void*arg){
    threadpool*pool=(threadpool*)arg;
    while(!pool->shutdown){
        sleep(3);
        //取出线程池中任务数量和存活线程数量
        pthread_mutex_lock(&pool->mutexpool);
           int queuesize=pool->queuesize;
              int livenum=pool->livenum;
              pthread_mutex_unlock(&pool->mutexpool);
           //取出忙线程数量
              pthread_mutex_lock(&pool->mutexbusy);
                int busynum=pool->busynum;
                pthread_mutex_unlock(&pool->mutexbusy);
        //创建新线程
        if(queuesize>livenum&&livenum<pool->maxnum){
            pthread_mutex_lock(&pool->mutexpool);
            int counter=0;
            for(int i=0;i<pool->maxnum&&counter<NUMBER&&pool->livenum<pool->maxnum;i++){
                if(pool->threadids[i]==0){
                    pthread_create(&pool->threadids[i],NULL,worker,pool);
                    counter++;
                    pool->livenum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexpool);
        }
            //释放线程
            if(busynum*2<livenum&&livenum>pool->minnum){
                pthread_mutex_lock(&pool->mutexpool);
                pool->exitnum=NUMBER;
                pthread_mutex_unlock(&pool->mutexpool);
                //让工作线程自杀
                for(int i=0;i<NUMBER;i++){
                    pthread_cond_signal(&pool->notempty);
            }
        }
    }
    return NULL;
}
void threadexit(threadpool*pool){
    pthread_t tid=pthread_self();
    for(int i=0;i<pool->maxnum;i++){
        if(pool->threadids[i]==tid){
            pool->threadids[i]=0;
            printf("thread %ld exit\n",tid);
            break;
        }
    }
    pthread_exit(NULL);
}
void taskfunc(void*arg){
    int num=*(int*)arg;
    printf("thread %ld working on task %d\n",pthread_self(),num);
    sleep(1);
}
int main(){
    threadpool*pool=threadpoolcreate(3,10,100);
      for(int i=0;i<100;i++){
        int*arg=(int*)malloc(sizeof(int));
        *arg=i;
        threadpooladd(pool,taskfunc,arg);
      }
      sleep(100);
        threadpooldestroy(pool);
      return 0;
}
