// threadpool.cpp 动态线程池
// 编译命令：g++ -std=c++17 -pthread threadpool.cpp -o tp
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
using namespace std;
// 作用：自动推导提交进去的函数的返回值类型，兼容老C++和C++17
// __cpp_lib_is_invocable 是编译器预定义宏，用来判断支不支持invoke_result_t
#if defined(__cpp_lib_is_invocable) && __cpp_lib_is_invocable >= 201703L
template<class F,class...Args>
using tp_invoke_result_t = invoke_result_t<F,Args...>;
#else
template<class F,class...Args>
using tp_invoke_result_t=typename result_of<F(Args...)>::type;
#endif
class ThreadPool{
    public:
        explicit ThreadPool(
            size_t minthreads=1,
            size_t maxthreads=max(2u,thread::hardware_concurrency()),
            size_t queuecapacity=1024,
            chrono::milliseconds idletimeout=chrono::seconds(30)
        ):m_min(minthreads),m_max(maxthreads),m_capacity(queuecapacity),m_idletimeout(idletimeout)
        {//校验
            if(m_max==0)throw invalid_argument("ThreadPool: maxThreads 必须 > 0");
            if (m_min > m_max) throw invalid_argument("ThreadPool: minThreads 不能大于 maxThreads");
            if (m_capacity == 0) throw invalid_argument("ThreadPool: queueCapacity 必须 > 0");
            lock_guard<mutex>lock (m_mutex);
            for(size_t i=0;i<m_min;i++){
             if(!spawnLocked())break;
            }
         }
         // 析构：线程池对象销毁的时候自动关闭线程池
         ~ThreadPool(){
             try{
                 shutdown();
             }
             catch(...){}
         }
        // =delete：禁用拷贝构造、赋值，线程池不能复制一份
        ThreadPool(const ThreadPool&)=delete;
        ThreadPool& operator=(const ThreadPool&)=delete;
         // submit 模板函数，提交任务
         template<class F,class...Args>
         future<tp_invoke_result_t<F,Args...>>submit(F&&f,Args&&...args){
             using r=tp_invoke_result_t<F,Args...>;
             auto task=make_shared<packaged_task<r()>>(bind(forward<F>(f),forward<Args>(args)...));
             future<r>fut=task->get_future();
             {
                 unique_lock<mutex>lock(m_mutex);
                 m_notfull.wait(lock,[this]{return m_stopping||m_tasks.size()<m_capacity;});
                 if(m_stopping)throw runtime_error("ThreadPool: 已关闭，不能再提交任务");
                 m_tasks.emplace_back([task]{(*task)();});
                 if(m_tasks.size()>m_idle&&m_alive<m_max)spawnLocked();
             }
             m_notempty.notify_one();
             return fut;
         }
         void shutdown(){
             unique_lock<mutex>lock(m_mutex);
             if(!m_stopping){
                 m_stopping=1;
                 m_notempty.notify_all();
                 m_notfull.notify_all();
             }
             m_idlecv.wait(lock,[this]{return m_alive==0;});
         }
         size_t aliveCount() const   { lock_guard<mutex> g(m_mutex); return m_alive; }
     size_t busyCount() const    { lock_guard<mutex> g(m_mutex); return m_busy; }
     size_t pendingCount() const { lock_guard<mutex> g(m_mutex); return m_tasks.size(); }
     size_t capacity() const     { return m_capacity; }
     void waitIdle(){
         unique_lock<mutex>lock(m_mutex);
         m_idlecv.wait(lock,[this]{return m_tasks.empty()&&m_busy==0;});
     }
     private:
         void workloop(){
             while(1){
                 unique_lock<mutex>lock(m_mutex);
                 ++m_idle;
                 const bool woken=m_notempty.wait_for(lock,m_idletimeout,[this]{return m_stopping||!m_tasks.empty();});
                 --m_idle;
                 if(!woken){
                     if(m_alive>m_min){
                         --m_alive;
                         m_idlecv.notify_all();
                         return;
                     }
                     continue;
                 }
                 if(m_tasks.empty()){
                     --m_alive;
                     m_idlecv.notify_all();
                     return;
                 }
                 auto task = move(m_tasks.front());
                 m_tasks.pop_front();
                 ++m_busy;                   
                  m_notfull.notify_one();  
                 lock.unlock();
                 try{
                     task();
                 }catch(...){
                 }
                 lock.lock();
                 --m_busy;
                 if(m_busy==0&&m_tasks.empty())m_idlecv.notify_all();
             }
         }
         bool spawnLocked(){
             if(m_alive>=m_max)return false;
             try{
                 thread t(&ThreadPool::workloop,this);
                 t.detach();
                 ++m_alive;
                 return true;
             }catch(const exception&){
                 return false;
             }
         }
             mutable mutex m_mutex;
             // mutable：即使在const成员函数里，也可以修改这个锁
             condition_variable m_notempty;
             condition_variable m_notfull;
             condition_variable m_idlecv;
             deque<function<void()>>m_tasks;
             const size_t m_min;                    // 最小线程数（常驻）
     const size_t m_max;                    // 最大线程数上限
     const size_t m_capacity;               // 任务队列最大容量
     const chrono::milliseconds m_idletimeout; // 空闲超时时间

     size_t m_alive = 0;                    // 当前一共存活的线程总数
     size_t m_idle = 0;                     // 当前处于空闲等待的线程数量
     size_t m_busy = 0;                     // 当前正在执行任务的线程数量
     bool m_stopping = false;               // 标记：线程池是否正在关闭
};
int main(){
    ThreadPool pool(2,8,64,chrono::milliseconds(500));
    cout << "[init] alive=" << pool.aliveCount() << "\n";
    vector<future<long long>>futures;
    for(int i=0;i<40;i++){
        futures.push_back(pool.submit([i]{
            this_thread::sleep_for(chrono::milliseconds(20));
            return static_cast<long long>(i)*i;
        }));
    }
    long long sum=0;
    for(auto &f:futures)sum+=f.get();
     cout << "[calc] sum=" << sum << " (expect 20540)\n";
     auto boom=pool.submit([]()->int{throw runtime_error("boom");});
     try{
         boom.get();
     }catch(const exception& e){
         cout << "[exception] " << e.what() << "\n";
     }
         cout << "[stats] alive=" << pool.aliveCount()
         << " busy=" << pool.busyCount()
         << " pending=" << pool.pendingCount() << "\n";

     pool.waitIdle();
     this_thread::sleep_for(chrono::milliseconds(1200));
     cout << "[shrink] alive=" << pool.aliveCount() << " (expect 2)\n";
     atomic<int>hits(0);
     for(int i=0;i<10;i++){
         pool.submit([&hits]{
             hits++;
         });
     }
     pool.waitIdle();
      cout << "[void] hits=" << hits.load() << " (expect 10)\n";
      for(int i=0;i<5;i++){
          pool.submit([i]{
              this_thread::sleep_for(chrono::milliseconds(30));
              cout << ("  drain " + to_string(i) + "\n");
          });
      }
       pool.shutdown();
     cout << "[shutdown] alive=" << pool.aliveCount() << " (expect 0)\n";
     try{
         pool.submit([](){});
     }catch(const exception& e){
cout << "[closed] " << e.what() << "\n";
     }
     return 0;
}
