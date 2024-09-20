#include "cc.h"

namespace cc{

static cc::Logger::ptr g_logger = CC_LOG_NAME("system");

//拥有协程调度器之后，协程中的回调函数成员实际上是scheduler的run函数，即协程的入口函数中执行的就是run函数
//调度器只有一个，在main(不管是否使用use_caller，main都是调度线程)中创建，因此如果在其他线程中，t_scheduler应该是nullptr
//只有在main中，t_scheduler才等于Getthis(); 对应于stop()中的那个判断条件；
//使用caller线程进行调度的过程:
//1.main函数主协程运行，创建调度器
//2.仍然是main函数主协程运行，向调度器添加一些调度任务
//3.开始协程调度，main函数主协程让出执行权，切换到调度协程，
//  调度协程从任务队列里按顺序执行所有的任务
//4.每次执行一个任务，调度协程都要让出执行权，再切到该任务的协程里去执行，
//  任务执行结束后，还要再切回调度协程，继续下一个任务的调度
//5.所有任务都执行完后，调度协程还要让出执行权并切回main函数主协程，以保证程序能顺利结束。
static thread_local Scheduler* t_scheduler = nullptr;
//线程调度协程
static thread_local Fiber* t_scheduler_fiber = nullptr;


Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
    : m_name(name){
    CC_ASSERT(threads > 0);

    //使用调度器所在线程，可以少创建一个线程
    if(use_caller) {
        CC_LOG_INFO(g_logger) << "use_caller";
        cc::Fiber::GetThis();
        --threads;
        //自己本身没有调度器
        CC_ASSERT(GetThis() == nullptr); 
        t_scheduler = this;
        //rootFiber用于处理调度器的run函数
        //非静态成员函数需要传递this指针作为第一个参数，用 std::bind()进行绑定
        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
        cc::Thread::SetName(m_name);

        t_scheduler_fiber = m_rootFiber.get();
        m_rootThread = cc::GetThreadId();
        m_threadIds.push_back(m_rootThread);
    } else {
        m_rootThread = -1; 
    }
    m_threadCount = threads;
}

Scheduler::~Scheduler(){
    CC_ASSERT(m_stopping);
    if(GetThis() == this){
        t_scheduler = nullptr;
    }
}

Scheduler* Scheduler::GetThis(){
    return t_scheduler;
}

Fiber* Scheduler::GetMainFiber(){
    return t_scheduler_fiber;
}

//功能概述：
//根据需要创建的线程数量，创建线程池，每创建一个，就去执行对应的调度函数
//只用caller所在的线程的话，start实际上被延迟到stop才开始
void Scheduler::start(){
    CC_LOG_INFO(g_logger) << "start()";
    MutexType::Lock lock(m_mutex);
    if(!m_stopping){
        return;
    }
    m_stopping = false;
    //CC_ASSERT(m_threads.empty());

    m_threads.resize(m_threadCount);
    //调度线程创建好，就立刻开始处理任务
    for(size_t i = 0; i < m_threadCount; ++i){
        m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), 
                                m_name + "_" + std::to_string(i)));
        m_threadIds.push_back(m_threads[i]->getId());
    }
    lock.unlock();
    // if(m_rootFiber){ //使用的当前线程的主协程
    //     //m_rootFiber->swapIn();
    //     m_rootFiber->call();
    //     CC_LOG_INFO(g_logger) << "call out " << m_rootFiber->getId();
    // }
}

void Scheduler::stop(){
    m_autostop = true;
    //使用use_caller且只有一个线程
    if(m_rootFiber 
                && m_threadCount == 0
                && (m_rootFiber->getState() == Fiber::TERM
                || m_rootFiber->getState() == Fiber::INIT)) {
        CC_LOG_INFO(g_logger) << this << " stopped";
        m_stopping = true;
        
        if(stopping()){
            //返回到main所在线程继续执行
            return;
        }
        //如果还有任务，继续往下执行
    }

    //如果use caller，那只能由caller线程发起stop
    //即如果使用了main所在的线程进行调度，那么只能等待main线程的调度器
    //其他线程事实上没有调度器
    if(m_rootThread != -1){
        CC_ASSERT(GetThis() == this);
    } else {
        CC_ASSERT(GetThis() != this);
    }

    m_stopping = true;
    for(size_t i = 0; i < m_threadCount; ++i){
        tickle();
    }

    //使用use_caller多tickle一下，因为use_caller没有记在m_threadCount这个数量里
    if(m_rootFiber){
        tickle();
    }

    if(m_rootFiber){
        // while(!stopping()){
        //     if(m_rootFiber->getState() == Fiber::TERM
        //         || m_rootFiber->getState() == Fiber::EXCEPT){
        //         m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
        //         CC_LOG_INFO(g_logger) << " root fiber is term, reset";
        //         t_scheduler_fiber = m_rootFiber.get();
        //     }
        //     m_rootFiber->call();
        // }
        if(!stopping()){//main函数所在的线程作为主线程时，此时才会去main函数的任务
            m_rootFiber->call();
        }
    }

    std::vector<Thread::ptr> thrs;
    {
        MutexType::Lock lock(m_mutex);
        thrs.swap(m_threads);
    }

    for(auto& i : thrs){
        i->join();
    }
}

void Scheduler::setThis(){
    t_scheduler = this;
}

//每个创建的线程绑定的是run: 协程调度主函数
//  在非caller线程里，调度协程就是调度线程的主协程，
//  但在caller线程里，调度协程并不是caller线程的主协程，其也相当于caller线程的一个子协程，
//  这在协程切换时会有麻烦
//  如果只使用caller线程进行调度，那所有的任务协程都在stop之后排队调度。
//  如果有额外线程，那任务协程在刚添加到任务队列时就可以得到调度。
void Scheduler::run(){
    //Fiber::GetThis();
    CC_LOG_INFO(g_logger) << "run";
    set_hook_enable(true);
    setThis();
    //不是main所在的线程，那么协程的主协程就是正在执行run函数的协程
    if(cc::GetThreadId() != m_rootThread){
        t_scheduler_fiber = Fiber::GetThis().get();
    }

    //没有协程有任务做时空闲协程执行
    Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
    //待调度协程，用于接收下面ft中可能需要调度的协程
    Fiber::ptr cb_fiber;
    //待调度的任务
    FiberAndThread ft;
    while(true){
        ft.reset();
        bool tickle_me = false;
        bool is_active = false;
        {
            MutexType::Lock lock(m_mutex);
            //m_fibers即为任务队列
            auto it = m_fibers.begin();
            //找到一个需要执行的协程就可以退出
            while (it != m_fibers.end()) {
                //如果已经指定了线程但是当前线程并不是被指定的,tickle即可，跳过
                if(it->thread != -1 && it->thread != cc::GetThreadId()){ 
                    it++;
                    tickle_me = true;
                    continue;
                }

                //有任务可调度(协程 / 函数)
                CC_ASSERT(it->fiber || it->cb);
                //待调度的是协程，且这个协程在执行，跳过
                if(it->fiber && it->fiber->getState() == Fiber::EXEC){
                    ++it;
                    continue;
                }

                //拿到这个任务(没在执行，且当前线程就是它绑定的线程或者没有指定线程)
                ft = *it;
                m_fibers.erase(it);
                ++m_activeThreadCount;
                is_active = true;
                break;
            }
            //有需要调度的协程
            tickle_me |= it != m_fibers.end();
        }

        if(tickle_me){
            tickle();
        }

        //需要调度的是协程且状态正常
        if(ft.fiber && (ft.fiber->getState() != Fiber::TERM
                        && ft.fiber->getState() != Fiber::EXCEPT)){
            //切换到这个协程
            ft.fiber->swapIn();
            //执行完/或者暂时被HOLD
            --m_activeThreadCount;
            //如果未执行完，根据状态进行选择，继续加入调度队列或者HOLD
            if(ft.fiber->getState() == Fiber::READY){
                schedule(ft.fiber);
            } else if (ft.fiber->getState() != Fiber::TERM 
                        && ft.fiber->getState() != Fiber::EXCEPT){
                ft.fiber->m_state = Fiber::HOLD;
            }
            //执行结束
            ft.reset();
        } else if(ft.cb){ //需要调度的是回调函数，包装为协程进行调度
            if(cb_fiber){
                cb_fiber->reset(ft.cb);
            } else {
                cb_fiber.reset(new Fiber(ft.cb));
            }
            ft.reset();
            cb_fiber->swapIn();
            --m_activeThreadCount;
            //与协程类似
            if(cb_fiber->getState() == Fiber::READY){
                schedule(cb_fiber);
                cb_fiber.reset();
            } else if (cb_fiber->getState() == Fiber::TERM //执行结束(正常中止或者异常)
                        || cb_fiber->getState() == Fiber::EXCEPT){
                cb_fiber->reset(nullptr);
            } else {//if (cb_fiber->getState() != Fiber::TERM ){
                cb_fiber->m_state = Fiber::HOLD;
                cb_fiber.reset();
            }
            cb_fiber.reset();
        } else {
            //没有任务时，不断执行轮询，使用idle_fiber占用CPU，
            if(is_active){
                --m_activeThreadCount;
                continue;
            }
            // 如果idle_fiber的状态为TERM则完全结束调度
            if(idle_fiber->getState() == Fiber::TERM){
                CC_LOG_INFO(g_logger) << "idle fiber term";
                //continue;
                break;
            }
            //CC_LOG_INFO(g_logger) << "idle thread ID = " << cc::GetThreadId();
            // 否则，运行idle协程
            ++m_idleThreadCount;
            std::cout << "---------------" << std::endl;
            idle_fiber->swapIn();
            --m_idleThreadCount;
            if(idle_fiber->getState() != Fiber::TERM
                && idle_fiber->getState() != Fiber::EXCEPT){
                idle_fiber->m_state = Fiber::HOLD;
            }
        }
    }
}

void Scheduler::tickle(){
    CC_LOG_INFO(g_logger) << "tickle";
}

// 当自动停止 && 正在停止 && 任务队列为空 && 活跃的线程数量为0
bool Scheduler::stopping(){
    MutexType::Lock lock(m_mutex);
    return m_autostop && m_stopping && m_fibers.empty() && m_activeThreadCount == 0;
}

// 协程无任务可调度时执行idle协程,暂时占用CPU，不停的判断stopping是否满足
void Scheduler::idle(){
   CC_LOG_INFO(g_logger) << "Scheduler's idle";
   while(!stopping()){
        cc::Fiber::YieldToHold();
   }
}


}