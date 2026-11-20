#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#define NUMBER 2//每次创建或销毁的线程数
//任务结构体
typedef struct task{
    void(*function)(void*arg);
    void*arg;
}task;
/* ---------- 线程池 ---------- */
 typedef struct threadpool{
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
   int shutdown;
      pthread_cond_t condzero;
} threadpool;
   //线程池是否关闭
  
// 前置声明
void* worker(void* arg);
void* manager(void* arg);
void worker_leave(threadpool*pool){
    pool->livenum--;
    pthread_cond_broadcast(&pool->condzero);
    pthread_mutex_unlock(&pool->mutexpool);
    pthread_exit(NULL);
}
// ---------- 创建线程池 ----------
threadpool*threadpoolcreate(int min,int max,int queuesize){
    if(min<1||max<min||queuesize<1){
        fprintf(stderr, "threadpoolcreate: 参数非法 min=%d max=%d queuesize=%d\n",
                min, max, queuesize);
                return NULL;
    }
    threadpool*pool=(threadpool*)calloc(1,sizeof(threadpool));
    if(pool==NULL){
        fprintf(stderr,"malloc threadpool fail\n");
        return NULL;
    }
    int ok_mutexpool=0,ok_mutexbusy=0;
    int ok_notfull=0,ok_notempty=0,ok_condzero=0;
    pool->taskq=(task*)calloc((size_t)queuesize,sizeof(task));
    if(pool->taskq==NULL){
        fprintf(stderr,"malloc taskq fail\n");
        goto fail;
    }
    if(pthread_mutex_init(&pool->mutexpool,NULL)!=0)goto fail;
    ok_mutexpool=1;
    if(pthread_mutex_init(&pool->mutexbusy,NULL)!=0)goto fail;
    ok_mutexbusy=1;
    if(pthread_cond_init(&pool->notfull,NULL)!=0)goto fail;
    ok_notfull=1;
    if(pthread_cond_init(&pool->notempty,NULL)!=0)goto fail;
    ok_notempty=1;
    if(pthread_cond_init(&pool->condzero,NULL)!=0)goto fail;
    ok_condzero=1;
    pool->queuecapacity=queuesize;
    pool->minnum=min;
    pool->maxnum=max;
    pool->queuesize=0;
    pool->queuefront=0;
    pool->queuerear=0;
    pool->busynum=0;
    pool->livenum=0;
    pool->exitnum=0;
    pool->shutdown=0;
    if(pthread_create(&pool->managerid,NULL,manager,pool)!=0){
        fprintf(stderr,"create manager fail\n");
        goto fail;
    }
    for(int i=0;i<min;i++){
        pthread_t tid;
        if(pthread_create(&tid,NULL,worker,pool)!=0){
            fprintf(stderr,"create worker fail\n");
            continue;
        }
            pool->livenum++;
        pthread_detach(tid);
    }
    return pool;
    fail:
    if(ok_mutexpool)pthread_mutex_destroy(&pool->mutexpool);
    if(ok_mutexbusy)pthread_mutex_destroy(&pool->mutexbusy);
    if(ok_notfull)pthread_cond_destroy(&pool->notfull);
    if(ok_notempty)pthread_cond_destroy(&pool->notempty);
    if(ok_condzero)pthread_cond_destroy(&pool->condzero);
    free(pool->taskq);
    free(pool);
    return NULL;
}
int threadpooldestroy(threadpool**ppool){
    if(ppool==NULL||*ppool==NULL)return -1;
    threadpool*pool=*ppool;
    pthread_mutex_lock(&pool->mutexpool);
    if(pool->shutdown){
        pthread_mutex_unlock(&pool->mutexpool);
        *ppool=NULL;
        return -1;}
    pool->shutdown=1;
    pthread_cond_broadcast(&pool->notempty);
    pthread_cond_broadcast(&pool->notfull);
    pthread_mutex_unlock(&pool->mutexpool); 
    pthread_join(pool->managerid,NULL);//先停掉扩容/缩容
    pthread_mutex_lock(&pool->mutexpool);
    while(pool->livenum>0){//等worker真正全部退出
        pthread_cond_wait(&pool->condzero,&pool->mutexpool);
    }
    //防御性清理：正常流程worker会排空队列，这里兜底释放残留arg
    while(pool->queuesize>0){
        free(pool->taskq[pool->queuefront].arg);
        pool->queuefront=(pool->queuefront+1)%pool->queuecapacity;
        pool->queuesize--;
    }
    pthread_mutex_unlock(&pool->mutexpool);
    free(pool->taskq);
    pool->taskq=NULL;
    pthread_mutex_destroy(&pool->mutexpool);
    pthread_mutex_destroy(&pool->mutexbusy);
    pthread_cond_destroy(&pool->notfull);
    pthread_cond_destroy(&pool->notempty);
    pthread_cond_destroy(&pool->condzero);
    free(pool);
    *ppool=NULL;
    return 0;
}
int threadpooladd(threadpool*pool,void(*func)(void*arg),void*arg){//向任务队列中添加任务
   if(pool==NULL||func==NULL)return -1;
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
    pthread_mutex_lock(&pool->mutexbusy);
    int busy=pool->busynum;
    pthread_mutex_unlock(&pool->mutexbusy);
    return busy;
}
int threadpoolalive(threadpool*pool){
    pthread_mutex_lock(&pool->mutexpool);
    int alive=pool->livenum;
    pthread_mutex_unlock(&pool->mutexpool);
    return alive;
}
//任务全部执行完毕
int threadpoolidle(threadpool*pool){
  if(pool==NULL)return 1;
    pthread_mutex_lock(&pool->mutexpool);
    pthread_mutex_lock(&pool->mutexbusy);//加锁顺序固定为mutexpool->mutexbusy
    int idle=(pool->queuesize==0&&pool->busynum==0);
    pthread_mutex_unlock(&pool->mutexbusy);
    pthread_mutex_unlock(&pool->mutexpool);
    return idle;
}
void*worker(void*arg){//工作线程
    threadpool*pool=(threadpool*)arg;
    while(1){
        pthread_mutex_lock(&pool->mutexpool);
        while(pool->queuesize==0&&!pool->shutdown&&pool->exitnum==0){
            pthread_cond_wait(&pool->notempty,&pool->mutexpool);
        }
         if(pool->queuesize==0){
            if(pool->shutdown){
                worker_leave(pool);
            }
            if(pool->exitnum>0){
                pool->exitnum--;
                if(pool->livenum>pool->minnum){
                    worker_leave(pool);
                }
            }
            pthread_mutex_unlock(&pool->mutexpool);
            continue;
         }
        //取出任务
        task task=pool->taskq[pool->queuefront];
        pool->queuefront=(pool->queuefront+1)%pool->queuecapacity;
        pool->queuesize--;
        pthread_mutex_lock(&pool->mutexbusy);
        pool->busynum++;
        pthread_mutex_unlock(&pool->mutexbusy);
        pthread_cond_signal(&pool->notfull);
        pthread_mutex_unlock(&pool->mutexpool);
        printf("thread %ld start working\n",pthread_self());
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
        if(pool->shutdown){
            pthread_mutex_unlock(&pool->mutexpool);
            break;
        }
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
            for(int i=0;i<NUMBER&&pool->livenum<pool->maxnum;i++){
                pthread_t tid;
               pool->livenum++;
                if(pthread_create(&tid,NULL,worker,pool)==0){
                    pthread_detach(tid);
                }else{
                    pool->livenum--;
                    break;
                }
            }
            pthread_mutex_unlock(&pool->mutexpool);
        }
            //释放线程
            if(queuesize==0&&busynum*2<livenum&&livenum>pool->minnum){
                pthread_mutex_lock(&pool->mutexpool);
               int want=pool->livenum-pool->minnum;
               pool->exitnum=want>NUMBER?NUMBER:want;
                pthread_mutex_unlock(&pool->mutexpool);
                pthread_cond_broadcast(&pool->notempty);
        }
    }
    return NULL;
}
// ---------- 测试 ----------
void taskfunc(void*arg){
  int num = *(int *)arg;
    printf("thread %lu working on task %d\n", (unsigned long)pthread_self(), num);
    sleep(1);
}

int main(){
    threadpool*pool=threadpoolcreate(3,10,100);
    if(pool==NULL)return 1;
      for(int i=0;i<100;i++){
        int *arg = (int *)malloc(sizeof(int));
        if (arg == NULL) break;
        *arg = i;
        if (threadpooladd(pool, taskfunc, arg) != 0) {  /* [FIX] 检查入队结果 */
            free(arg);                                 /* 入队失败，自己释放 */
            break;
        }
}
while(!threadpoolidle(pool)){
    usleep(100000);
}
 printf("all tasks done, busy=%d alive=%d\n", threadpoolbusy(pool), threadpoolalive(pool));
 threadpooldestroy(&pool);
 printf("thread pool destroyed\n");
 return 0;
}
