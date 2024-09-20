#ifndef __CC_IOMANAGER_H__
#define __CC_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

//实现协程调度
//封装了epoll，支持为socket fd注册读写事件回调函数
//IO协程调度器使用一对管道fd来tickle调度协程
//IO协程调度支持协程调度的全部功能，因为IO协程调度器是直接继承协程调度器实现的。
//除了协程调度，IO协程调度还增加了IO事件调度的功能，这个功能是针对描述符（一般是套接字描述符）的。
//IO协程调度支持为描述符注册可读和可写事件的回调函数，当描述符可读或可写时，执行对应的回调函数。
//可以直接把回调函数等效成协程，所以这个功能被称为IO协程调度
namespace cc{

class IOManager : public Scheduler, public TimerManager{
public:

    using ptr = std::shared_ptr<IOManager>;
    using RWMutexType = RWMutex; 

    enum Event {
        NONE  = 0x000,
        READ  = 0X001, // = EPOLLIN
        WRITE = 0X004, // = EPOLLOUT
    };
private:
    // 文件描述符的上下文类
    // 实际是一个三元组，包含: 描述符-事件类型-回调函数
    struct FdContext{ 
        using MutexType = Mutex;
        //fd的每个事件都有一个事件上下文，保存这个事件的回调函数以及执行回调函数的调度器
        //其中描述符fd和事件类型(EPOLLIN/EPOLLOUT)用于epoll_wait，回调函数用于协程调度。
        //只预留了读事件和写事件，所有的事件都被归类到这两类事件中
        struct EventContext{
            Scheduler* scheduler = nullptr;       //执行事件回调的scheduler
            Fiber::ptr fiber;                     //事件回调协程
            std::function<void()> cb;             //事件的回调函数
        };

        //根据事件类型获取对应事件上下文
        EventContext& getcontext(Event event);
        //重置事件
        void resetContext(EventContext& ctx);
        //触发事件
        void triggerEvent(Event event);

        EventContext read;      //读事件
        EventContext write;     //写事件
        int fd = 0;             //事件关联的句柄
        Event events = NONE;    //fd所关注的事件类型
        MutexType mutex;        //事件的mutex
    };
    
public:
    /**
     * 构造函数
     * threads 线程数量
     * use_caller 是否将调度器所在线程包含进去
     * name 调度器的名称
     */
    IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "");
    ~IOManager();

    /**
     * 添加事件
     * fd描述符发生了event事件时执行cb函数
     * fd socket句柄
     * event 事件类型
     * cb 事件回调函数，如果为空，则默认把当前协程作为回调执行体
     * 添加成功返回0,失败返回-1
     */
    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    bool delEvent(int fd, Event event);
    bool cancelEvent(int fd, Event event);

    bool cancelAll(int fd);

    static IOManager* GetThis();

protected:
    //有新事件需要唤醒协程执行tickle()
    void tickle() override;
    //是否需要终止
    bool stopping() override;
    //没有协程需要执行时，执行idle
    void idle() override;
    //根据超时时间判断是否中止
    bool stopping(uint64_t& timeout);
    //重置context上下文大小
    void contextResize(size_t size);
    //
    void onTimerInsertedAtFront() override;
private:
    //epoll 文件句柄
    //文件句柄是操作系统用于标识和管理已打开文件或其他I/O资源的抽象概念。
    //在Unix/Linux系统中，文件句柄称为文件描述符，通常是一个非负整数。
    int m_epfd = 0;
    //用于tickle
    //pipe 文件句柄
    //fd[0]读取 fd[1]写入
    int m_tickleFds[2];
    //当前等待执行的事件数量
    std::atomic<size_t> m_pendingEventCount = {0};
    RWMutexType m_mutex;
    //socket事件上下文的容器
    std::vector<FdContext*> m_fdContexts;
};

}


#endif