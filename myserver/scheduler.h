#ifndef __CC_SCHEDULER_H__
#define __CC_SCHEDULER_H__

#include <memory>
#include "fiber.h"
#include <mutex>
#include "thread.h"
#include <functional>
#include <list>
#include <vector>
#include <iostream>

namespace cc{

//协程调度器，既可以调度线程也可以直接调度协程(函数)
//调度器的任务：
//  调度器内部维护一个任务队列和一个调度线程池。
//  开始调度后，线程池从任务队列里按顺序取任务执行。
//  调度线程可以包含caller线程。当全部任务都执行完了，
//  线程池停止调度，等新的任务进来。
//  添加新任务后，通知线程池有新的任务进来了，
//  线程池重新开始运行调度。停止调度时，各调度线程退出，调度器停止工作。

//  解决之前协程模块中子协程不能运行另一个子协程的缺陷，
//  子协程可以通过向调度器添加调度任务的方式来运行另一个子协程。
class Scheduler{

public:
    //friend class cc::Fiber;
    using ptr = std::shared_ptr<Scheduler>;
    using MutexType = Mutex;
    // 线程数量
    // use_caller 是否使用调度器所在的线程进行协程调用，这样可以少创建一个线程，效率更高
    // 调度器所在的线程(称为caller线程)main函数所在的线程
    Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "");

    virtual ~Scheduler();

    const std::string& getName() const {return m_name;}

    static Scheduler* GetThis();
    static Fiber* GetMainFiber();

    //启动任务调度，根据线程数创建线程并开始调度
    void start();
    void stop();

    //添加调度任务 fc
    //thread: 协程执行的线程id,-1则不指定线程
    template<class FiberOrCb>
    void schedule(FiberOrCb fc, int thread = -1){ 
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            //将一个待调度任务放到调度器的带调度协程队列中
            need_tickle = scheduleNoLock(fc, thread);
        }

        //调度队列为空才需要通知，否则不需要通知
        if(need_tickle){
            tickle();
        }
    }
    
    //支持添加一批任务
    template<class InputIterator>
    void schedule(InputIterator begin, InputIterator end){
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            while (begin != end)
            {
                need_tickle = scheduleNoLock(&*begin, -1) || need_tickle;
                ++begin;
            }
        }
        if(need_tickle) {
            tickle();
        }
    }
protected:
                            
    virtual void tickle();  //通知协程调度器有任务
    void run();             //执行协程调度
    virtual bool stopping();//返回是否可以停止
    virtual void idle();    //协程无任务可调度时执行idle协程,暂时占用CPU

    void setThis();

    bool hasIdleThreads() {return m_idleThreadCount > 0;}
private:
    //如果当前任务队列为空，则需要通知: need_tickle = true
    template<class FiberOrCb>
    bool scheduleNoLock(FiberOrCb fc, int thread){
        bool need_tickle = m_fibers.empty();
        FiberAndThread ft(fc, thread);
        if(ft.fiber || ft.cb){
            //放入调度队列
            m_fibers.push_back(ft);
        }
        return need_tickle;
    }

private:
    //任务结构体
    struct FiberAndThread{
        Fiber::ptr fiber;
        std::function<void()> cb;
        int thread; //线程号

        FiberAndThread(Fiber::ptr f, int thr)
            :fiber(f), thread(thr){

        }

        FiberAndThread(Fiber::ptr* f, int thr)
            : thread(thr){
            //因为传入的是一个智能指针，我们使用后会造成引用数加一，
            //可能会引发释放问题，这里swap相当于把传入的智能指针变成一个空指针
            //此时f实际已经没用了
            fiber.swap(*f);
        }

        FiberAndThread(std::function<void()> f, int thr)
            :cb(f), thread(thr){

        }

        FiberAndThread(std::function<void()>* f, int thr)
            :thread(thr){
            cb.swap(*f);
        }

        FiberAndThread()
            : thread(-1){

        }

        void reset(){
            fiber = nullptr;
            thread = -1;
            cb = nullptr;
        }
    };

private:
    MutexType m_mutex;
    //线程池，线程依次从m_fibers中取出任务并执行
    std::vector<Thread::ptr> m_threads;
    //待执行的协程队列
    std::list<FiberAndThread> m_fibers;
    //use_caller为true时有效,调度器所在线程的调度协程
    Fiber::ptr m_rootFiber;
    std::string m_name;

protected:
    //协程下的线程id数组
    std::vector<int> m_threadIds;
    //线程数量
    size_t m_threadCount = 0;
    //工作线程数量
    std::atomic<size_t> m_activeThreadCount {0};
    //空闲线程数量
    std::atomic<size_t> m_idleThreadCount {0};
    //是否正在停止
    bool m_stopping = true;
    //是否自动停止
    bool m_autostop = false;
    //主线程id
    int m_rootThread = 0; 
};

}

#endif
