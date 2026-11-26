# threadpool

C 语言 + pthread 实现的**动态线程池**：常驻最小线程数，管理者线程按负载自动扩缩容，任务进环形队列由空闲线程领取执行。

## 特性

- 固定 `min` 个常驻线程，忙时最多扩到 `max`，空闲自动回落
- 环形任务队列，满时入队阻塞（背压）
- 线程创建即 `detach`，销毁时等所有线程真正退出再释放内存（无 UAF / 无泄漏）
- 共享状态全部加锁，锁顺序固定 `mutexpool → mutexbusy`
- 通过 ThreadSanitizer / AddressSanitizer

## 编译运行

```sh
gcc -std=gnu11 -Wall -Wextra -pthread -g -O2 -o task task.c
./task
