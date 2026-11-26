# threadpool

一个用 C++17 写的动态线程池，单文件实现，基于 `std::thread` / `mutex` / `condition_variable`。

常驻少量工作线程，任务堆积时自动扩容，空闲超时后自动缩容；提交任务返回 `std::future`，可拿返回值、可捕获异常。

## 特性

- **动态扩缩容**：`submit` 时队列长度超过空闲线程就补线程（上限 `max`），空闲超过 `idleTimeout` 自动退出（下限 `min`）
- **返回 future**：`submit(f, args...)` 返回 `std::future<R>`，支持任意返回值（含 `void`）和异常传播
- **优雅关闭**：`shutdown()` 先排空队列、等所有线程退出，再销毁对象；析构函数自动调用
- **线程安全**：单 `mutex` + 三个条件变量，判空与唤醒都在锁内完成，无丢失唤醒
- **零依赖**：只用标准库，一个 `.cpp` 文件

## 编译运行

```bash
g++ -std=c++17 -O2 -pthread task.cpp -o threadpool
./threadpool
```
要求：C++17 及以上、支持 pthread 的平台（Linux / macOS / WSL / MinGW-w64）。


## API

| 接口 | 说明 |
|---|---|
| `ThreadPool(min, max, capacity, idleTimeout)` | 构造线程池，立即启动 `min` 个工作线程 |
| `submit(f, args...)` | 提交任务，返回 `std::future<R>`；队列满时阻塞 |
| `waitIdle()` | 阻塞到“队列为空且无线程执行任务” |
| `shutdown()` | 优雅关闭，阻塞到所有线程退出；可重复调用 |
| `aliveCount()` | 当前存活线程数 |
| `busyCount()` | 正在执行任务的线程数 |
| `pendingCount()` | 队列中待执行任务数 |
| `capacity()` | 队列容量 |

## 设计要点

- **没有独立的管理者线程**：扩容发生在 `submit` 内（`m_tasks.size() > m_idle` 就 `spawnLocked()`），提交高峰响应更快。
- **缩容靠超时**：worker 用 `wait_for(idleTimeout)` 等任务，超时且 `m_alive > m_min` 就自行退出。
- **任务包装**：`std::bind` 绑定参数 → `std::packaged_task<R()>` → 用 `shared_ptr` 包一层（`packaged_task` 只能移动）→ 存成 `std::function<void()>` 入队。
- **关闭顺序**：置 `m_stopping` → `notify_all` → 等 `m_alive == 0`，保证析构时没有线程还在访问 `this`。

## 注意事项

1. **参数是拷贝进闭包的**（`std::bind` 语义）。要修改外部变量用 `std::ref`，跨线程传引用要保证生命周期。
2. **`future.get()` 才会重抛任务里的异常**，不取结果的异常会被静默丢弃。
3. **只能移动的参数**（如 `unique_ptr`）提交时用 `std::move`。
4. `using namespace std;` 只适合单文件小项目；拆成头文件时建议去掉。
## 测试

```bash
# 数据竞争 / 内存错误
g++ -std=c++17 -g -O1 -pthread -fsanitize=thread  task.cpp -o tp_tsan && ./tp_tsan
g++ -std=c++17 -g -O1 -pthread -fsanitize=address task.cpp -o tp_asan && ./tp_asan
```

示例程序覆盖：批量任务求和（40 个任务、`max=8`）、异常传播、扩缩容统计、`shutdown` 后拒绝提交。
