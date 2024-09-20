#include "fiber.h"
#include <atomic>
#include "config.h"
#include "macro.h"
#include "log.h"
#include "scheduler.h"

//协程模块主要包括协程的构造，析构，切换，执行
//构造分为主协程和普通协程 (在不使用main线程的情况下，主协程 = 调度协程)
//主协程不分配栈空间
//子协程绑定一个协程入口函数(mainfunc)，当执行该协程时，
//切换到其入口函数，并在其中执行其cb(协程具体实现)
//切换基于swapcontext执行，关注切换时当前上下文保存给谁

namespace cc
{

static Logger::ptr g_logger = CC_LOG_NAME("system");

//全局静态变量，用于生成协程id
static std::atomic<uint64_t> s_fiber_id {0};
//全局静态变量，用于统计当前的协程数
static std::atomic<uint64_t> s_fiber_count {0};

//不同线程中的协程互不影响
//当前线程正在运行的协程
static thread_local Fiber* t_fiber = nullptr;
//当前线程中的主协程
static thread_local Fiber::ptr t_threadFiber = nullptr;

static ConfigVar<uint32_t>::ptr g_fiber_stack_size = 
    Config::Lookup<uint32_t>("fiber.stack_size", 128 * 1024, "fiber stack size");


class MallocStackAllocator{
public:
    static void* Alloc(size_t size){
        return malloc(size);
    }

    static void Dealloc(void *vp, size_t size){
        return free(vp);
    }

};

using StackAllocator = MallocStackAllocator;

uint64_t Fiber::GetFiberId(){
    if(t_fiber){
        return t_fiber->getId();
    }
    return 0;
}

//无参构造函数只用于创建线程的第一个协程，也就是主协程
//主协程
Fiber::Fiber(){

    m_state = EXEC;
    //将当前协程设置为正在执行
    SetThis(this);
    //getcontext(ucontext_t *ucp):
    //获取当前上下文, 并将其保存到ucp指针所指的结构中。

    if(getcontext(&m_ctx)){
        CC_ASSERT2(false, "getcontext");
    }

    ++s_fiber_count;
    CC_LOG_DEBUG(g_logger) << "Fiber::Fiber main";
}

//构造子协程；所有协程的入口函数都是一样的MainFunc或者CallerMainFunc
//构造函数参数包含入口函数，栈大小
Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool use_caller) 
    :m_id(++s_fiber_id)
    ,m_cb(cb){

    ++s_fiber_count;
    m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();
    m_stack = StackAllocator::Alloc(m_stacksize);
    if(getcontext(&m_ctx)){
        CC_ASSERT2(false, "getcontext");
    }

    //uc_link指向下一个需要调度的协程
    //对于普通协程，只需要切换回主协程
    m_ctx.uc_link = nullptr;
    //当前上下文的栈指针
    m_ctx.uc_stack.ss_sp = m_stack;
    //当前上下文的栈空间大小
    m_ctx.uc_stack.ss_size = m_stacksize;

    //void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...);
    //  修改由getcontext获取到的上下文指针ucp，将其与一个函数func进行绑定，支持指定func运行时的参数，argc: 函数入口参数的个数
    //  在调用makecontext之前，必须手动给ucp分配一段内存空间，存储在ucp->uc_stack中，这段内存空间将作为func函数运行时的栈空间，
    //  同时也可以指定ucp->uc_link，表示函数运行结束后恢复uc_link指向的上下文，
    //  如果不赋值uc_link，那func函数结束时必须调用setcontext或swapcontext以重新指定一个有效的上下文，否则程序就跑飞了
    //  makecontext执行完后，ucp就与函数func绑定了，调用setcontext或swapcontext激活ucp时，func就会被运行
    
    //不使用main所在的线程,绑定主协程(此时主协程就是调度协程)
    if(!use_caller){
        makecontext(&m_ctx, &Fiber::MainFunc, 0);
    }else{
        makecontext(&m_ctx, &Fiber::CallerMainFunc, 0);
    }
    
    CC_LOG_DEBUG(g_logger) << "Fiber::Fiber id = " << m_id;
}

Fiber::~Fiber(){
    --s_fiber_count;
    if(m_stack){
        //这些状态的协程都可以被析构
        CC_ASSERT(m_state == TERM || m_state == EXCEPT || m_state == INIT);
        //回收空间
        StackAllocator::Dealloc(m_stack, m_stacksize);
    } else {
        //线程中的主协程: 没有栈也没有cb，只负责处理生成子协程
        CC_ASSERT(!m_cb);
        CC_ASSERT(m_state == EXEC);
        Fiber* cur = t_fiber;
        //当前协程为主协程，置空
        if(cur == this) {
            SetThis(nullptr);
        }
    }

    CC_LOG_DEBUG(g_logger) << "Fiber::~Fiber id = " << m_id;
}

//重置协程函数，及状态，由新的cb重新获取之前的栈空间
//INIT, TERM
//重置内存，或者该协程执行完，但是可以使用栈中分配的空间继续执行
void Fiber::reset(std::function<void()> cb){
    CC_ASSERT(m_stack);
    CC_ASSERT(m_state == TERM || m_state == EXCEPT || m_state == INIT);

    m_cb = cb;
    if(getcontext(&m_ctx)){
        CC_ASSERT2(false, "getcontext");
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;
    
    //绑定的是协程入口函数，里面封装了cb
    //MainFunc中会执行cb
    makecontext(&m_ctx, &Fiber::MainFunc, 0);
    m_state = INIT;
}


// int swapcontext(ucontext_t *oucp, const ucontext_t *ucp);
// 恢复ucp指向的上下文，同时将当前的上下文存储到oucp中，
// 和setcontext一样，swapcontext也不会返回，而是会跳转到ucp上下文对应的函数中执行，相当于调用了函数
// swapcontext是sylar非对称协程实现的关键，线程主协程和子协程用这个接口进行上下文切换
// 主协程切换到当前协程
// 关于swapcontext如何切换，例如在某个函数执行中，f1调用了swap，那么会将当前函数的上下文保存在oucp，切换到f1的上下文
void Fiber::call(){
    SetThis(this);
    m_state = EXEC;
    CC_LOG_ERROR(g_logger) << getId();
    if(swapcontext(&t_threadFiber->m_ctx, &m_ctx)){
        CC_ASSERT2(false, "swapcontext");
    }
}

// 普通协程执行back()
void Fiber::back(){
    SetThis(t_threadFiber.get());
    if (swapcontext(&m_ctx, &t_threadFiber->m_ctx)){
        CC_ASSERT2(false, "swapcontext");
    }
}

//调度协程切换到当前协程
void Fiber::swapIn(){

    SetThis(this);
    CC_ASSERT(m_state != EXEC);
    m_state = EXEC;
    if(swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx)) {
        CC_ASSERT2(false, "swapcontext");
    }
    //未引入调度器
    // if(swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {
    //     CC_ASSERT2(false, "swapcontext");
    // }
}

//切换到后台执行，调度协程切换到主协程
void Fiber::swapOut(){
    SetThis(Scheduler::GetMainFiber());
    if (swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx)){
        CC_ASSERT2(false, "swapcontext");
    }
    // 未引入调度器
    // if (swapcontext(&m_ctx, &t_threadFiber->m_ctx)){
    //     CC_ASSERT2(false, "swapcontext");
    // }
}  

//设置当前协程
void Fiber::SetThis(Fiber *f){
    t_fiber = f;
}

//返回当前协程
//没有则新创建一个作为当前协程和线程的主协程
Fiber::ptr Fiber::GetThis(){
    if(t_fiber){
        return t_fiber->shared_from_this();
    }

    Fiber::ptr main_fiber(new Fiber);
    CC_ASSERT(t_fiber == main_fiber.get());
    t_threadFiber = main_fiber;
    return t_fiber->shared_from_this();
}

//切换回主协程，设置为Ready状态
void Fiber::YieldToReady(){
    Fiber::ptr cur = GetThis();
    cur->m_state = READY;
    cur->swapOut();
}

//切换回主协程，设置为Ready状态
void Fiber::YieldToHold(){
    Fiber::ptr cur = GetThis();
    cur->m_state = HOLD;
    cur->swapOut();
}

//总协程数
uint64_t Fiber::TotalFibers(){
    return s_fiber_count;
}

//协程入口函数的封装，主要目的是为了实现当函数调用结束之后可以主动返回主协程
void Fiber::MainFunc(){
    Fiber::ptr cur = GetThis();
    CC_ASSERT(cur);
    try{
        //执行任务
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = TERM;
    }catch(std::exception& ex){
        cur->m_state = EXCEPT;
        CC_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
                               << "fiber_id = " << cur->GetFiberId()
                               << std::endl
                               << BackTraceToString();
    }catch(...){
        cur->m_state = EXCEPT;
        CC_LOG_ERROR(g_logger) << "Fiber Except"
                               << "fiber_id = " << cur->GetFiberId()
                               << std::endl
                               << BackTraceToString();
    }
    //返回裸指针
    auto raw_ptr = cur.get();
    //cur -> nullptr
    cur.reset();
    //切换回主协程
    //使用智能指针的话，存在释放问题
    raw_ptr->swapOut();
    CC_ASSERT2(false, "never reach");
}
//和MainFunc基本一致，除了返回使用的是back()，而不是swapout()
void Fiber::CallerMainFunc(){
    Fiber::ptr cur = GetThis();
    CC_ASSERT(cur);
    try{
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = TERM;
    }catch(std::exception& ex){
        cur->m_state = EXCEPT;
        CC_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
                               << "fiber_id = " << cur->GetFiberId()
                               << std::endl
                               << BackTraceToString();
    }catch(...){
        cur->m_state = EXCEPT;
        CC_LOG_ERROR(g_logger) << "Fiber Except"
                               << "fiber_id = " << cur->GetFiberId()
                               << std::endl
                               << BackTraceToString();
    }

    auto raw_ptr = cur.get();
    cur.reset();
    //切换回调度协程(caller专用)
    raw_ptr->back();
    //cur->swapOut();
    CC_ASSERT2(false, "never reach");
}

} // namespace cc
