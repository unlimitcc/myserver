#include "iomanager.h"
#include "log.h"
#include "macro.h"
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

namespace cc{

static cc::Logger::ptr g_logger = CC_LOG_NAME("system");

IOManager::FdContext::EventContext& IOManager::FdContext::getcontext(Event event){
    switch(event){
        case IOManager::READ:
            return read;
        case IOManager::WRITE:
            return write;
        default:
            CC_ASSERT2(false, "getcontext");
    }
}

void IOManager::FdContext::resetContext(EventContext& ctx){
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;
}

void IOManager::FdContext::triggerEvent(Event event){
    //关注的事件中包含要触发的事件
    CC_ASSERT(events & event);
    //从关注事件中删除触发的事件
    events = (Event)(events & ~event);
    EventContext& ctx = getcontext(event);
    if(ctx.cb){
        ctx.scheduler->schedule(&ctx.cb);
    } else {
        ctx.scheduler->schedule(&ctx.fiber);
    }
    ctx.scheduler = nullptr;
    return;
}

//改造协程调度器，使其支持epoll
IOManager::IOManager(size_t threads, bool use_caller, const std::string& name)
        : Scheduler(threads, use_caller, name) {
    //epoll_create()是用于创建一个新的epoll实例的系统调用，
    //返回一个文件描述符(epoll 文件描述符),
    //该文件描述符可以用于管理和监控其他文件描述符的事件。
    //参数 5000 事实上无实际意义
    m_epfd = epoll_create(5000);
    CC_ASSERT(m_epfd > 0);

    //pipe的参数是数组
    //  m_tickleFds[0] 用于读取
    //  m_tickleFds[1] 用于写入，
    //保存的即为对应的文件描述符
    //pipe管道创建成功返回0,失败返回-1
    int rt = pipe(m_tickleFds);
    CC_ASSERT(!rt);

    //源码
    //struct epoll_event
    //{
    //  uint32_t events;	    /* Epoll events */
    //  epoll_data_t data;	    /* User data variable */
    //} __EPOLL_PACKED;
    // epoll_event是一个结构体:
    // uint32_t events	用于标识感兴趣的事件类型（如可读、可写、错误等）
    // epoll_data_t 是一个联合体，用于存储用户自定义的数据，通常是文件描述符（fd）或者指向某个对象的指针。
    // 使用 epoll 进行事件监听时，可以为每个监听的文件描述符（如套接字、管道等）附加一些自定义数据，
    // 这些数据通过 epoll_data_t 传递给用户，以便在事件发生时可以方便地获取与该事件关联的信息

    //typedef union epoll_data {
    //  void *ptr;          // 用户自定义的指针
    //  int fd;             // 文件描述符
    //  uint32_t u32;       // 32位无符号整数
    //  uint64_t u64;       // 64位无符号整数
    //} epoll_data_t;
    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    //注册读事件并且支持边缘触发
    event.events = EPOLLIN | EPOLLET;
    //注册pipe读句柄的可读事件 
    event.data.fd = m_tickleFds[0];

    //fcntl 是一个用于操作文件描述符的函数
    //1: 文件描述符，用于指定要操作的文件描述符。
    //2: F_GETFD：获取文件描述符标志。
    //   F_SETFD：设置文件描述符标志。常用的标志：
    //   FD_CLOEXEC：在执行 exec 系列函数时关闭该文件描述符。
    //   F_GETFL：获取文件状态标志。
    //   F_SETFL：设置文件状态标志。常用的标志:
    //      O_NONBLOCK：非阻塞模式。
    //      O_APPEND：追加模式
    //将管道的读文件描述符设置为非阻塞模式，配合边缘触发
    //在读取不到数据或是写入缓冲区已满会马上return，而不会阻塞等待
    //注: 边缘触发必须配合非阻塞
    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
    CC_ASSERT(!rt);

    //epoll_ctl: 用于向 epoll 实例中添加、修改或删除文件描述符
    //1: epoll实例的文件描述符
    //2: EPOLL_CTL_ADD：向 epoll 实例中添加一个新的文件描述符。
    //   EPOLL_CTL_MOD：修改已存在于 epoll 实例中的文件描述符的事件。
    //   EPOLL_CTL_DEL：从 epoll 实例中删除一个文件描述符。
    //3: 需要添加、修改或删除的目标文件描述符。
    //4: 指向 epoll_event 结构体的指针，该结构体包含了要监听的事件类型和相关的数据。
    //   对于 EPOLL_CTL_DEL 操作，该参数可以为 NULL。
    //此时若管道可读，epoll_wait会返回
    //将pipe的读端注册到epoll
    rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    
    CC_ASSERT(!rt);
    //初始化socket事件上下文vector
    contextResize(32);

    //scheduler的start方法，IOManager创建完成即开始调度
    start();
}

IOManager::~IOManager(){
    stop();
    //close 函数是一个基本且重要的系统调用，用于关闭文件描述符，释放系统资源。
    CC_LOG_INFO(g_logger) << "[~IOManager] stop end";
    close(m_epfd); 
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);
    //删除管理的所有事件描述符
    for(size_t i = 0; i < m_fdContexts.size(); ++i){
        if(m_fdContexts[i]){
            delete m_fdContexts[i];
        }
    }
}

void IOManager::contextResize(size_t size){
    m_fdContexts.resize(size);
    for(size_t i = 0; i < m_fdContexts.size(); ++i){
        if(!m_fdContexts[i]){
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;
        }
    }
}

//添加，删除，都是先拿到fd，拿到对应的事件，在epoll实例中修改，之后修改fdctx
int IOManager::addEvent(int fd, Event event, std::function<void()> cb){
    
    FdContext* fd_ctx = nullptr;
    RWMutexType::ReadLock lock(m_mutex);
    //fd是否超出范围
    if((int)m_fdContexts.size() > fd){
        fd_ctx = m_fdContexts[fd];
        lock.unlock();
    } else {
        lock.unlock();
        RWMutexType::WriteLock lock2(m_mutex);
        contextResize(fd * 1.5);
        fd_ctx = m_fdContexts[fd];
    }

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    
    //要添加的事件已经被添加了
    //一个句柄一般不会重复加同一个事件，可能是两个不同的线程在操控同一个句柄添加事件
    if(fd_ctx->events & event){
        CC_LOG_ERROR(g_logger) << "addEvent assert fd=" << fd
                               << " event=" << event
                               << "fd_ctx.event=" << fd_ctx->events;
        CC_ASSERT(!(fd_ctx->events & event));
    }

    //已有事件，则使用修改, 没有事件, 则使用添加
    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    //EPOLLET 边缘触发, 添加原有事件及新事件
    epevent.events = EPOLLET | fd_ctx->events | event;
    epevent.data.ptr = fd_ctx;

    //注册事件
    //0 -> success | -1 -> error
    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt){
        CC_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd <<", "
                               << op << "," << fd << ", " << epevent.events
                               << "):" << rt << " (" << errno << ") (" 
                               << strerror(errno) << ")";
        return -1;
    }

    ++m_pendingEventCount;

    //更新fd_ctx的事件
    fd_ctx->events = (Event)(fd_ctx->events | event);
    //获得对应事件的 EventContext(三元组)
    FdContext::EventContext& event_ctx = fd_ctx->getcontext(event);
    //EventContext的成员应该都为空
    CC_ASSERT(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb);
    event_ctx.scheduler = Scheduler::GetThis();
    //有回调函数，则添加回调函数，没有则将当前协程作为执行体添加
    if(cb){
        event_ctx.cb.swap(cb);
    }else{
        event_ctx.fiber = Fiber::GetThis();
        CC_ASSERT(event_ctx.fiber->getState() == Fiber::EXEC);
    }
    return 0;
}

bool IOManager::delEvent(int fd, Event event){
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd){
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    //若没有要删除的事件
    if(!(fd_ctx->events & event)){ 
        return false;
    }

    //将事件从注册事件中删除
    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt){
        CC_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd <<", "
                               << op << "," << fd << ", " << epevent.events
                               << "):" << rt << " (" << errno << ") (" 
                               << strerror(errno) << ")";
        return false;
    }

    --m_pendingEventCount;
    fd_ctx->events = new_events;
    //拿到对应事件的上下文，并重置。因为一个文件描述符关注多个事件
    //所以可能还有其他事件
    FdContext::EventContext& event_ctx = fd_ctx->getcontext(event);
    fd_ctx->resetContext(event_ctx);
    return true;
}
//和删除基本一致，区别在于最后会触发一下当前事件
bool IOManager::cancelEvent(int fd, Event event){
    RWMutexType::ReadLock lock(m_mutex);
    if(m_fdContexts.size() <= fd){
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!(fd_ctx->events & event)){
        return false;
    }

    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt){
        CC_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd <<", "
                               << op << "," << fd << ", " << epevent.events
                               << "):" << rt << " (" << errno << ") (" 
                               << strerror(errno) << ")";
        return false;
    }

    //取消之前触发一次
    fd_ctx->triggerEvent(event);
    --m_pendingEventCount;
    return true;
}

bool IOManager::cancelAll(int fd){
    RWMutexType::ReadLock lock(m_mutex);
    if(m_fdContexts.size() <= fd){
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!fd_ctx->events){
        return false;
    }

    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    //errno 是一个全局变量，用于存储最近一次系统调用或库函数调用发生错误时的错误代码。
    //在C和C++编程中，许多标准库函数在失败时不会返回详细的错误信息，
    //而是通过设置 errno 来指示错误类型
    if(rt){
        CC_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd <<", "
                               << op << "," << fd << ", " << epevent.events
                               << "):" << rt << " (" << errno << ") (" 
                               << strerror(errno) << ")";
        return false;
    }

    //全部触发一次
    if(fd_ctx->events & READ){
        fd_ctx->triggerEvent(READ);
        --m_pendingEventCount;
    }

    if(fd_ctx->events & WRITE){
        fd_ctx->triggerEvent(WRITE);
        --m_pendingEventCount;
    }

    CC_ASSERT(fd_ctx->events == 0);
    return true;
}

IOManager* IOManager::GetThis(){
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle(){
    //无空闲线程
    if(!hasIdleThreads()){
        return;
    }
    //向pipe中写入一个"T",此时pipe会从epoll_wait中唤醒
    int rt = write(m_tickleFds[1],"T",1);
    CC_ASSERT(rt == 1);
}

bool IOManager::stopping(){
    uint64_t timeout = 0;
    return stopping(timeout);
}

bool IOManager::stopping(uint64_t& timeout){
    timeout = getNextTimer();
    return timeout == ~0ull
            && m_pendingEventCount == 0
            && Scheduler::stopping();
}

//对于IO协程调度来说，应阻塞在等待IO事件上，
//  idle退出的时机是epoll_wait返回，对应的操作是tickle或注册的IO事件就绪
//  调度器无调度任务时会阻塞idle协程上。
//idle状态应该关注两件事，
//一是有没有新的调度任务，对应Schduler::schedule()，
//  如果有新的调度任务，那应该立即退出idle状态，并执行对应的任务；
//二是关注当前注册的所有IO事件有没有触发，如果有触发，那么应该执行
//  IO事件对应的回调函数。
void IOManager::idle(){ //P38
    const uint64_t MAX_EVNETS = 256;
    epoll_event* events = new epoll_event[MAX_EVNETS]();
    std::shared_ptr<epoll_event> shared_events(events ,[](epoll_event* ptr){
        delete[] ptr;
    });
    std::cout << "------idle------" << std::endl;
    //int rt = 0;
    while(1){
        //下一个任务要执行的时间
        uint64_t next_timeout = 0;
        CC_LOG_INFO(g_logger) << "timeout = " << next_timeout;
        if(stopping(next_timeout)) {
            CC_LOG_INFO(g_logger) << "name=" << getName() 
                                    << " idle stopping exit";
            break;
        }
        int rt = 0;

        //陷入epoll_wait，等待事件发生
        do{ //重置超时时间，最大为MAX_TIMEOUT
            static const int MAX_TIMEOUT = 3000;//ms
            //有指定的超时时间
            if(next_timeout != ~0ull){
                next_timeout = (int)next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
            }else{//没有指定超时时间，设置为3000ms
                next_timeout = MAX_TIMEOUT;
            }
            

            //epfd epoll_create() 返回的句柄
            //events 分配好的 epoll_event 结构体数组，epoll 将会把发生的事件复制到 events 数组中
            //events不可以是空指针，内核只负责把数据复制到这个 events 数组中，
            //不会去帮助我们在用户态中分配内存，但是内核会检查空间是否合法。
            //maxevents 表示本次可以返回的最大事件数目，通常 maxevents 参数与预分配的 events 数组的大小是相等的；
            //timeout 表示在没有检测到事件发生时最多等待的时间（单位为毫秒）
            //如果 timeout 为 0，则表示 epoll_wait 在 rdllist 链表为空时，立刻返回，不会等待。
            //  rdllist:所有已经ready的epitem(表示一个被监听的fd)都在这个链表里面
            //第2个参数 events 是一个数组，epoll_wait 会将发生的事件填充到这个数组中。
            //  next_timeout = -1时表示无限等待

            //1.超时时间到了
            //2.关注的socket有数据来了
            //3.通过tickle往pipe里发数据，表明有任务来了
            rt = epoll_wait(m_epfd, events, MAX_EVNETS, (int)next_timeout);
            // rt表示返回值为正整数表示发生事件的文件描述符的数量。
            // 这意味着有n个文件描述符已经准备好进行I/O操作，并且这些事件已经被写入events数组。
            // 操作系统中断会返回EINTR，然后重新epoll_wait
            if(rt < 0 && errno == EINTR){
            } else {
                // 有就绪事件发生
                break;
            }
        }while(1);

        // 有就绪事件发生
        // 这里调用listExpiredCb返回的应该是那些超时的定时器
        // 因为有刚刚超时的，所以需要去执行
        std::vector<std::function<void()> > cbs;
        listExpireCb(cbs);
        if(!cbs.empty()){
            // 把超时任务全部加入调度器
            schedule(cbs.begin(), cbs.end());
            cbs.clear();
        }

        // 处理就绪的fd
        for(int i = 0; i < rt; ++i){
            epoll_event& event = events[i];
            // 如果获得的这个信息是来自pipe
            if(event.data.fd == m_tickleFds[0]){
                // 读一个byte到dummy中
                uint8_t dummy;
                while(read(m_tickleFds[0], &dummy, 1) == 1);
                continue;
            }

            FdContext* fd_ctx = (FdContext*)event.data.ptr;
            FdContext::MutexType::Lock lock(fd_ctx->mutex);
            // 事件发生了错误或者已经挂起(注册事件时内核会自动关注EPOLLERR和EPOLLHUP)
            // EPOLLERR 表示对应的文件描述符发生错误
            // EPOLLHUP 表示对应的文件描述符被挂起
            if(event.events & (EPOLLERR | EPOLLHUP)){ 
                // 同时触发读写事件
                event.events |= EPOLLIN | EPOLLOUT;
            }
            //获取感兴趣的事件(读/写)
            int real_events = NONE;
            // 读事件
            if(event.events & EPOLLIN){
                real_events |= READ; 
            }   
            // 写事件
            if(event.events & EPOLLOUT){
                real_events |= WRITE;
            }
            // 没有事件
            if((fd_ctx->events & real_events) == NONE){
                continue;
            }
            // 获取剩余事件,并重新注册
            int left_events = (fd_ctx->events & ~real_events); 
            int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            event.events = EPOLLET | left_events;
            int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
            if(rt2){
                CC_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd <<", "
                                << op << "," << fd_ctx->fd << ", " << event.events
                                << "):" << rt2 << " (" << errno << ") (" 
                                << strerror(errno) << ")";
                continue;
            }

            // 执行读 / 写事件
            if(real_events & READ){
                fd_ctx->triggerEvent(READ); 
                --m_pendingEventCount;
            }
            if(real_events & WRITE){
                fd_ctx->triggerEvent(WRITE);
                --m_pendingEventCount;
            }
        }

        //执行完epoll_wait返回的事件
        //获得当前协程
        Fiber::ptr cur = Fiber::GetThis();
        auto raw_ptr = cur;
        cur.reset();
        //返回主协程(调度器的MainFiber)，进行下一轮
        raw_ptr->swapOut();
    }
}

void IOManager::onTimerInsertedAtFront() {
    tickle();
}

}
 