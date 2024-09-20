#include "thread.h"
#include "log.h"
#include "util.h"

namespace cc{

//由于静态线程本地存储变量是线程特定的，因此它们的初始化和销毁时机也与普通静态变量不同。
//具体来说，在每个线程首次访问该变量时会进行初始化，在线程结束时才会进行销毁，
//而不是在程序启动或运行期间进行一次性初始化或销毁。
static thread_local Thread* t_thread = nullptr;//指向当前线程
static thread_local std::string t_thread_name = "UNKNOWN";

static cc::Logger::ptr g_logger = CC_LOG_NAME("system");//系统日志打印到system中

//int sem_init(sem_t *sem, int pshared, unsigned int value);
//pshared 用于决定信号量的共享行为：
//  pshared:为 0，表示信号量用于线程间的同步，即它是一个进程内的信号量。
//  pshared:为非零值，表示信号量用于进程间的同步，即它可以在不同的进程之间共享。
//      此时信号量通常会放在共享内存中。
//  value: 用于设置信号量的初始值。通常，信号量的值表示可用资源的数量。
//  return 0 -> success
Semaphore::Semaphore(uint32_t count){
    if(sem_init(&m_semaphore, 0, count)){
        throw std::logic_error("sem_init error");
    }
}

//注意，只有在确保没有任何线程或进程正在使用该信号量时，
//才应该调用 sem_destroy() 函数。否则，可能会导致未定义的行为。
//此外，如果在调用 sem_destroy() 函数之前，
//没有使用 sem_post() 函数将信号量的值增加到其初始值，
//则可能会导致在销毁信号量时出现死锁情况。
Semaphore::~Semaphore(){
    sem_destroy(&m_semaphore);
}

//使用信号量
//返回0，表示成功, !0表示失败
//sem_wait 函数会尝试将指定的信号量 sem 的值减 1。
//如果信号量的值 > 0，操作成功并返回，表示线程或进程成功进入临界区。
//如果信号量的值 = 0，则该函数会阻塞调用线程，直到信号量的值大于 0 时才返回（此时它会将信号量的值减 1）。
void Semaphore::wait(){ 
    if(sem_wait(&m_semaphore)){ 
        throw std::logic_error("sem_wait error");
    }
}

//归还信号量
//增加信号量的值
//sem_post 函数会将指定的信号量 sem 的值加 1。
//如果有线程或进程因 sem_wait 而阻塞，则其中一个会被唤醒，
//从而能够继续执行。这相当于释放一个资源或退出临界区。
void Semaphore::notify(){ 
    if(sem_post(&m_semaphore)){ 
        throw std::logic_error("sem_post error");
    }
}

//获取自身当前使用的线程
Thread* Thread::GetThis(){
    return t_thread;
}

//日志使用获取自身当前使用的线程名称
const std::string& Thread::GetName(){
    return t_thread_name;
} 

void Thread::SetName(const std::string& name){
    if(t_thread){
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

Thread::Thread(std::function<void()> cb, const std::string &name)
    : m_cb(cb),
      m_name(name){
    if(name.empty()){
       m_name = "UNKNOWN";
    }
    
    // 第一个参数为指向线程标识符(ID)的指针。
    // 第二个参数用来设置线程属性。
    // 第三个参数是线程运行函数的起始地址，
    //      该函数将在新线程中运行。该函数必须采用一个void类型的指针作为参数，并返回一个void类型的指针
    // 最后一个参数是运行函数的参数。
    // 新线程将在与调用pthread_create函数的线程并发执行的情况下运行。
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    CC_LOG_ERROR(g_logger) << "construct : " << cc::GetThreadId();
    if(rt){ //存在问题
        CC_LOG_ERROR(g_logger) << "pthread_create thread fail, rt = " << rt
                               << " name=" << name;
        throw std::logic_error("pthread_create error");
    }
    //阻塞等待线程真正运行起来
    m_semaphore.wait();
}

Thread::~Thread(){
    if(m_thread) {
        pthread_detach(m_thread);
    }
}

void* Thread::run(void* arg){
    Thread* thread = (Thread*)arg;
    t_thread = thread;
    //构造函数的wait保证能够正确初始化ID
    thread->m_id = cc::GetThreadId();
    CC_LOG_ERROR(g_logger) << "run : " << cc::GetThreadId();
    t_thread_name = thread->m_name;

    //线程重命名
    //(pthread_t, const char* name)
    pthread_setname_np(pthread_self(), thread->m_name.substr(0,15).c_str());
    std::function<void()> cb;
    cb.swap(thread->m_cb);
    //与Thread构造函数中的wait是一对，保证线程运行后再离开构造函数
    //线程的初始化已经完成，可以离开构造函数
    thread->m_semaphore.notify(); 

    cb();
    return 0;
}

// int pthread_join(pthread_t thread, void **retval);
//  void **retval: 指向一个指针的指针，用于存储被等待线程的退出状态。
//  如果不需要获取退出状态，可以传递NULL。
//  返回 0，表示线程成功地被等待并且其退出状态（如果有）已经被收集。
void Thread::join(){
    if(m_thread){
        int rt = pthread_join(m_thread, nullptr);
        if(rt){ //存在问题
            CC_LOG_ERROR(g_logger) << "pthread_join thread fail, rt = " << rt
                                << " name=" << m_name;
            throw std::logic_error("pthread_join error");
        }
        m_thread = 0;
    }
}


}