#include "config.h"
#include "hook.h"
#include <dlfcn.h>
#include "fiber.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "log.h"

cc::Logger::ptr g_logger = CC_LOG_NAME("system");
namespace cc{

static thread_local bool t_hook_enable = false;

static cc::ConfigVar<int>::ptr g_tcp_connect_timeout =
    cc::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt) \

//获取所有hook的函数原始接口
void hook_init(){
    static bool is_inited = false;
    if(is_inited){
        return;
    }
//name ## _f: ## 是 C/C++ 预处理器中的令牌粘贴运算符，
//用于将左右两边的标识符拼接成一个标识符。
//在这里，它将 name 与 _f 拼接成一个新的标识符。
//假设传递给 XX 宏的参数是 foo，那么 name ## _f 将被替换为 foo_f。
//RTLD_NEXT 是 dlopen 和 dlsym 函数的一个特殊标识符，用于在共享库中查找下一个定义的符号。

//dlsym - 从一个动态链接库或者可执行文件中获取到符号地址。成功返回跟name关联的地址
//RTLD_NEXT 表示查找链中下一个名称定义为 name的函数，即绕过当前定义查找下一个（通常是 libc 中的实现）。
//从当前调用位置之后的库中查找符号，而不是从全局符号库中查找。
//取出原函数，赋值给新函数
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX);
#undef XX
}

static uint64_t s_connect_timeout = -1;

struct _HookIniter{
    _HookIniter(){
        hook_init();
        s_connect_timeout = g_tcp_connect_timeout->getValue();

        //配置发生修改,进行通知
        g_tcp_connect_timeout->addListener([](const int& old_value, const int& new_value){
            CC_LOG_INFO(g_logger) << "tcp connect timeout changed from " << old_value 
                                  << " to " << new_value;
            s_connect_timeout = new_value;
        });
    }   
};
//静态对象，main函数之前就可以被初始化
static _HookIniter s_hook_initer;

bool is_hook_enable(){
    return t_hook_enable;
}

void set_hook_enable(bool flag){
    t_hook_enable = flag;
}


}
//定时器超时条件
//cancelled 表示是否取消
struct timer_info {
    int cancelled = 0;
};

/*
* fd: 文件描述符
* fun: 原始函数
* hook_fun_name: hook的函数名称
* event: 事件
* timeout_so: 超时时间类型
* args: 可变参数
*/
//do_io实现逻辑: 
//先排除不要执行异步的情况，例如用户没有设置hook，或者fd已经关闭或者不是socket类型，这时候直接执行原函数
//如果需要异步，先按照非阻塞模式执行原始API，如果是资源暂时不可用，那么将添加一个超时定时器(有回调函数)，
//并将事件加入IO管理器，之后让出执行权，转为HOLD。
//1.检查是否是有超时时间的异步，如果设置了超时时间，添加一个条件定时器，到超时时间还未回来，唤醒，设置
//当前状态为已超时，取消该事件(设置timer_info),返回
//2.如果没有超时时间或者在超时时间内已经正常返回，将事件添加进iom中等待对应的唤醒事件发生，
//发生后执行原始操作即可，此时对应事件已经就绪，不会阻塞
template<typename OriginFun, typename ... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name,
                     uint32_t event, int timeout_so, Args&&... args) {
    if(!cc::t_hook_enable) {
        return fun(fd, std::forward<Args>(args)...);
    }

    //CC_LOG_DEBUG(g_logger) << " do_io <" << hook_fun_name << " > ";
    //拿到fd对应的上下文
    cc::FdCtx::ptr ctx = cc::FdMgr::GetInstance()->get(fd);
    //没有文件调用原接口即可
    if(!ctx) {
        return fun(fd, std::forward<Args>(args)...);
    }
    //句柄已经关闭
    if(ctx->isClose()) {
        //坏文件描述符
        errno = EBADF;
        return -1;
    }
    //不是Socket或者用户设置了非阻塞，仍然调原接口
    if(!ctx->isSocket() || ctx->getUserNonBlock()) {
        return fun(fd, std::forward<Args>(args)...);
    }
    //以上都是不适用hook的情况
    //获得超时时间
    uint64_t to = ctx->getTimeout(timeout_so);
    //设置超时条件
    std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
    //先执行原始函数
    ssize_t n = fun(fd, std::forward<Args>(args)...);

    //EINTR表示系统调用在执行时被中断，例如被一个信号打断。
    while(n == -1 && errno == EINTR) {
        n = fun(fd, std::forward<Args>(args)...);
    }
    CC_LOG_DEBUG(g_logger) << " do_io <" << hook_fun_name << "> ";
    //需要重试
    //EAGAIN-（一般用于非阻塞的系统调用）
    //错误码 EAGAIN（或 EWOULDBLOCK） 表示资源暂时不可用，操作在非阻塞模式下无法立即完成
    //在这种情况下是很有可能出现发送缓冲区被填满
    //导致write\send无法再向缓冲区提交要发送的数据。
    //因此就产生了Resource temporarily unavailable的错误（资源暂时不可用）
    //EAGAIN 的意思也很明显，就是再次尝试

    //此时可以认为被阻塞了，可以设置一个定时器之后，切换其他协程执行
    if(n == -1 && errno == EAGAIN) {
        CC_LOG_DEBUG(g_logger) << " do_io <" << hook_fun_name << "> ";
        cc::IOManager* iom = cc::IOManager::GetThis();
        cc::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);

        //原Fdctx已经设置了超时时间
        if(to != (uint64_t)-1) {
            //to时间到了，依然没有消息则触发cb
            //如果提前唤醒，则winfo会被销毁，不会执行该回调
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event](){
                //如果对象存在，lock()函数返回一个指向共享对象的shared_ptr，否则返回一个空shared_ptr。
                auto t = winfo.lock();
                //定时器失效，说明该函数已经执行完成，函数中定义的局部变量全部被释放了
                if(!t || t->cancelled) {
                    return;
                }
                //没错误的话设置为超时而失败
                t->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd, (cc::IOManager::Event)(event));
            }, winfo);
        }
        
        //默认回调函数为空，将当前协程作为执行体
        int rt = iom->addEvent(fd, (cc::IOManager::Event)(event));
        //添加失败
        if(rt) {
            CC_LOG_ERROR(g_logger) << hook_fun_name << " addEvent("
                << fd << ", " << event << ")";
            if(timer) {
                timer->cancel();
            }
            return -1;
        } else {
            //addEvent成功，YieldToHold()把执行时间让出来，继续执行其他任务
            //只有两种情况会从这回来：
            // 1) 超时了， 174行取消timer的时候 triggerEvent会唤醒回来
            // 2) addEvent数据回来了会唤醒回来 
            CC_LOG_DEBUG(g_logger) << " do_io <" << hook_fun_name << "> ";
            cc::Fiber::YieldToHold();
            CC_LOG_DEBUG(g_logger) << " do_io <" << hook_fun_name << "> ";
            if(timer){
                timer->cancel();
            }
            //被超时计时器唤醒
            if(tinfo->cancelled){
                //errno = ETIMEDOUT
                errno = tinfo->cancelled;
                return -1;
            }
            //数据来了
            //被阻塞的事件满足了，重新执行原始的函数进行处理
            goto retry;
        }
    }
    return n;
}


extern "C"{

#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX

//添加一个定时器之后让出协程执行权
unsigned int sleep(unsigned int seconds){
    //没有启用hook，调用原始的接口
    if(!cc::t_hook_enable){
        return sleep_f(seconds);
    }

    cc::Fiber::ptr fiber = cc::Fiber::GetThis();
    cc::IOManager* iom = cc::IOManager::GetThis();

    //(void(sylar::Scheduler::*)(sylar::Fiber::ptr, int thread)) 是一个函数指针类型，
    //它定义了一个指向 sylar::Scheduler 类中一个参数为 sylar::Fiber::ptr 和 int 类型的成员函数的指针类型
    //参数iom是成员函数的调用对象，它不会作为成员函数的实际参数传递，而是指定哪个对象来调用该成员函数。
    //解释:
    //当绑定成员函数时，std::bind 的第一个参数是成员函数指针，第二个参数是调用该成员函数的对象或对象指针
    //schedule 调度任务为fiber，不指定线程
    //拿到cc::IOManager::schedule的地址，转为void(cc::Scheduler::*)(cc::Fiber::ptr, int thread)这个函数
    //指针类型
    iom->addTimer(seconds * 1000, 
                    std::bind((void(cc::Scheduler::*)
                    (cc::Fiber::ptr, int thread))&cc::IOManager::schedule, iom, fiber, -1));
    cc::Fiber::YieldToHold();
    return 0;
}

int usleep(useconds_t usec){
    if(!cc::t_hook_enable){
        return usleep_f(usec);
    }

    cc::Fiber::ptr fiber = cc::Fiber::GetThis();
    cc::IOManager* iom = cc::IOManager::GetThis();

    iom->addTimer(usec / 1000, [iom, fiber](){
        iom->schedule(fiber);
    });
    cc::Fiber::YieldToHold();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem){
    if(!cc::t_hook_enable){
        return nanosleep_f(req,rem);
    }
    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    cc::Fiber::ptr fiber = cc::Fiber::GetThis();
    cc::IOManager* iom = cc::IOManager::GetThis();

    iom->addTimer(timeout_ms, [iom, fiber](){
        iom->schedule(fiber);
    });
    cc::Fiber::YieldToHold();
    return 0;
}

//协议族为domain、协议类型为type(TCP UDP ..)、协议编号为protocol的套接字文件描述符
//调用成功，会返回一个标识这个套接字的文件描述符，失败的时候返回-1。
int socket(int domain, int type, int protocol){
    if(!cc::t_hook_enable){
        return socket_f(domain,type,protocol);
    }
    int fd = socket_f(domain,type,protocol);
    if(fd == -1) return fd;
    // 将fd放入到文件管理中
    cc::FdMgr::GetInstance()->get(fd, true);
    return fd;
}

int connect_with_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout_ms){
    if(!cc::t_hook_enable){
        return connect_f(fd, addr, addrlen);
    }
    //errno是一个全局变量，用于存储最近一次系统调用或库函数调用的错误代码。
    //当系统调用或库函数返回错误时，通常会设置 errno 为相应的错误码。
    //EBADF 通常表示“Bad file descriptor”错误，意思是操作尝试使用了无效的文件描述符。
    cc::FdCtx::ptr ctx = cc::FdMgr::GetInstance()->get(fd);
    if(!ctx || ctx->isClose()){
        errno = EBADF;
        return -1;
    }

    if(ctx->getUserNonBlock()){
        return connect_f(fd, addr, addrlen);
    }

    if(!ctx->isSocket()) {
        return connect_f(fd, addr, addrlen);
    }

    int n = connect_f(fd, addr, addrlen);
    //成功连接
    if(n == 0){
        return 0;
    }else if(n != -1 || errno != EINPROGRESS){//EINPROGRESS：代表连接建立需要时间，不能立即完成，而非阻塞会立即返回。(不一定失败)
        return n;
    }

    cc::IOManager* iom = cc::IOManager::GetThis();
    cc::Timer::ptr timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);
    std::weak_ptr<timer_info> winfo(tinfo);

    if(timeout_ms != (uint64_t)-1){
        timer = iom->addConditionTimer(timeout_ms, [winfo, fd, iom](){
            auto t = winfo.lock();
            if(!t || t->cancelled){
                return;
            }
            t->cancelled = ETIMEDOUT;
            iom->cancelEvent(fd, cc::IOManager::WRITE);
        }, winfo);
    }

    int rt = iom->addEvent(fd, cc::IOManager::WRITE);
    if(rt == 0){
        cc::Fiber::YieldToHold();
        //执行到此处说明被唤醒
        if(timer) {
            timer->cancel();
        }
        //被定时器唤醒，已经超时，任务失败
        if(tinfo->cancelled){
            errno = tinfo->cancelled;
            return -1;
        }
    }else{
        //添加事件失败
        if(timer){
            timer->cancel();
        }
        CC_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error"; 
    }

    //被唤醒之后查询socket的状态
    int error = 0;
    socklen_t len = sizeof(int);
    //获取套接字的错误状态
    if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)){
        return -1;
    }
    //没有错误，说明连接成功
    if(!error){
        return 0;
    }else{
        errno = error;
        return -1;
    }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
    return connect_with_timeout(sockfd, addr, addrlen, cc::s_connect_timeout);
}

//sockaddr用来处理网络通信的地址,socklen_t表示套接字地址的长度，返回值为成功建立连接的文件描述符
int accept(int s, struct sockaddr* addr, socklen_t *addrlen){
    int fd = do_io(s, accept_f, "accept", cc::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if(fd >= 0){
        //加入文件描述符管理集合
        cc::FdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void *buf, size_t count) {
    return do_io(fd, read_f, "read", cc::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, readv_f, "readv", cc::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return do_io(sockfd, recv_f, "recv", cc::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    return do_io(sockfd, recvfrom_f, "recvfrom", cc::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return do_io(sockfd, recvmsg_f, "recvmsg", cc::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return do_io(fd, write_f, "write", cc::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, writev_f, "writev", cc::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void *msg, size_t len, int flags) {
    return do_io(s, send_f, "send", cc::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    return do_io(s, sendto_f, "sendto", cc::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
    return do_io(s, sendmsg_f, "sendmsg", cc::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}


int close(int fd){
    if(!cc::t_hook_enable){
        return close_f(fd);
    }

    cc::FdCtx::ptr ctx = cc::FdMgr::GetInstance()->get(fd);
    if(ctx){
        auto iom = cc::IOManager::GetThis();
        if(iom){
            iom->cancelAll(fd);
        }
        cc::FdMgr::GetInstance()->del(fd);
    }
    return close_f(fd);
}

/*
fcntl 一个用于操作文件描述符的系统调用
arg: 可选参数，根据 cmd 的不同可能是整数、指针等。
va_list 是 C 标准库中用于处理可变参数函数（variadic function）的类型。
在 C++ 中也可以使用它来处理类似的情况。
可变参数函数允许传递不定数量和类型的参数给函数。
Eg: 定义：va_list va;
    va_list：用于存储可变参数的信息。
    va_start(va, cmd)： cmd是可变参数前的最后一个已知参数，初始化 va_list 变量。
    va_arg(va, int)：获取可变参数的下一个值。
    va_end(va)：结束可变参数的获取。
*/
int fcntl(int fd, int cmd, ... /* arg */ ){
    va_list va;
    va_start(va, cmd);
    switch(cmd) {
        case F_SETFL:
            {
                int arg = va_arg(va, int);
                va_end(va);
                cc::FdCtx::ptr ctx = cc::FdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClose() || !ctx->isSocket()) {
                    return fcntl_f(fd, cmd, arg);
                }
                ctx->setUserNonBlock(arg & O_NONBLOCK);
                if(ctx->getSysNonBlock()) {
                    arg |= O_NONBLOCK;
                } else {
                    arg &= ~O_NONBLOCK;
                }
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFL:
            {
                va_end(va);
                int arg = fcntl_f(fd, cmd);
                cc::FdCtx::ptr ctx = cc::FdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClose() || !ctx->isSocket()) {
                    return arg;
                }
                if(ctx->getUserNonBlock()) {
                    return arg | O_NONBLOCK;
                } else {
                    return arg & ~O_NONBLOCK;
                }
            }
            break;
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
            {
                int arg = va_arg(va, int);
                va_end(va);
                return fcntl_f(fd, cmd, arg); 
            }
            break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            {
                struct flock* arg = va_arg(va, struct flock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            {
                struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

//ioctl是设备驱动程序中对设备的I/O通道进行管理的函数
//ioctl(fd, FIONBIO, &value);
//  value为指向int类型的指针，如果该指针指向的值为0，则表示阻塞模式；
//  如果该指针指向的值为非0，则表示非阻塞模式。
int ioctl(int d, unsigned long int request, ...) {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    //FIONBIO用于设置文件描述符的阻塞模式的选项
    //根据其后的参数的非零值表示开启非阻塞模式
    if(FIONBIO == request) {
        bool user_nonblock = !!*(int*)arg; //例如 5通过 !!5 被转化为 true
        cc::FdCtx::ptr ctx = cc::FdMgr::GetInstance()->get(d);
        if(!ctx || ctx->isClose() || !ctx->isSocket()) {
            return ioctl_f(d, request, arg);
        }
        ctx->setUserNonBlock(user_nonblock);
    }
    return ioctl_f(d, request, arg);
}

//
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

/*
用于获取套接字的选项。
sockfd: 套接字描述符，标识要查询选项的套接字。
level: 指定套接字选项的层级。常见的层级包括：
    SOL_SOCKET: 套接字层,获取套接字的通用选项,如超时、缓冲区大小等。
    IPPROTO_TCP: TCP层,获取TCP特定选项。
    IPPROTO_IP: IP层,获取IP特定选项。
optname: 要查询的选项名称。例如，SO_RCVBUF（接收缓冲区大小）、SO_RCVTIMEO（接收超时）等。
optval: 指向存储选项值的内存区域。getsockopt 会将选项的值存放到这个位置。
optlen: 输入输出参数，指定 optval 指向的内存区域的长度。调用成功后，optlen 被更新为实际返回的选项值的长度。
*/
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if(!cc::t_hook_enable) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if(level == SOL_SOCKET) {
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            cc::FdCtx::ptr ctx = cc::FdMgr::GetInstance()->get(sockfd);
            if(ctx) {
                const timeval* v = (const timeval*)optval;
                //根据optname的具体选项设置对应的超时时间
                ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}
}
