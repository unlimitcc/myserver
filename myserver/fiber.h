#ifndef __CC_FIBER_H__
#define __CC_FIBER_H__

#include <ucontext.h>
#include <functional>
#include <memory>

namespace cc{

class Scheduler;

// 非对称协程模型，也就是子协程只能和线程主协程切换，
// 而不能和另一个子协程切换，并且在程序结束时，一定要再切回主协程
// 协程调度器调度主要思想为：先查看任务队列中有没有任务需要执行，
// 若没有任务需要执行则执行idle()，其思想主要在run()中体现。
class Fiber : public std::enable_shared_from_this<Fiber>{
friend class Scheduler;
public:
    
    using ptr = std::shared_ptr<Fiber>;

    enum State {
        //初始状态
        INIT,
        //暂停状态
        HOLD,
        //执行
        EXEC,
        //中止状态
        TERM,
        //就绪(可以执行)状态
        READY,
        EXCEPT
    };

private:
    // 每个线程第一个协程的构造
    Fiber();

public:
    //cb: 协程所执行的函数
    //stacksize: 协程栈大小
    //是否在Mainfiber上调度 
    Fiber(std::function<void()> cb, size_t stacksize = 0, bool use_caller = false);
    ~Fiber();

    //重置协程函数，及状态
    //INIT, TERM
    //重置内存，或者该协程执行完，但是可以使用栈中分配的空间继续执行
    void reset(std::function<void()> cb);
    
    //swapIn和swapOut是和调度器搭配使用的
    //调度协程切换到当前协程(如果不使用main所在的线程，调度协程就是主协程，负责是单独的调度协程)
    void swapIn();
    //切换到后台执行也可理解为切换到主协程
    void swapOut();
    
    //将当前线程切换到执行状态,目前只用在use_caller上
    //线程主协程切换到当前协程
    void call();
    //当前协程切换回主协程
    void back();

    uint64_t getId() const {return m_id;}
    State getState() const {return m_state;}
public:

    //设置当前协程
    static void SetThis(Fiber *f);

    //返回当前线程正在执行的协程
    //如果当前线程还未创建协程，则创建线程的第一个协程，
    //且该协程为当前线程的主协程，其他协程都通过这个协程来调度，
    //即,其他协程结束时,都要切回到主协程,由主协程重新选择新的协程进行切换
    //线程如果要创建协程，那么应该首先执行一下Fiber::GetThis()操作，以初始化主函数协程
    static Fiber::ptr GetThis();
    //当前协程切换到后台，设置为Ready状态
    static void YieldToReady();
    //当前协程切换到后台，设置为Hold状态
    static void YieldToHold();
    //总协程数
    static uint64_t TotalFibers();

    // 协程执行函数
    // 执行完成返回到 线程主协程
    static void MainFunc();
    // 协程执行函数
    // 执行完成返回到 线程调度协程(使用main线程时使用)
    static void CallerMainFunc();
    //获取协程id
    static uint64_t GetFiberId();
    State getState() {return m_state;}
private:

    //协程id
    uint64_t m_id = 0;
    //栈空间
    uint32_t m_stacksize = 0;
    State m_state = INIT;

    //上下文结构体定义
    //这个结构体是平台相关的，因为不同平台的寄存器不一样
    //下面列出的是所有平台都至少会包含的4个成员
    //  当前上下文结束后，下一个激活的上下文对象的指针，只在当前上下文是由makecontext创建时有效
    //      struct      ucontext_t *uc_link;
    //  当前上下文的信号屏蔽掩码
    //      sigset_t    uc_sigmask;
    //  当前上下文使用的栈内存空间，只在当前上下文是由makecontext创建时有效
    //      stack_t     uc_stack;
    //  包含栈指针uc_stack.ss_sp 和栈大小uc_stack.ss_size
    //  保存具体的程序执行上下文，如PC值，堆栈指针以及寄存器值等信息。
    //      mcontext_t  uc_mcontext;
    ucontext_t m_ctx;

    void* m_stack = nullptr;
    //协程运行函数
    std::function<void()> m_cb;
};

}

#endif