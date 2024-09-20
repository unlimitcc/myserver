#ifndef __CC_HOOK_H__
#define __CC_HOOK_H__

#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>

//hook系统底层和socket相关的API，socket IO相关的API，
//以及sleep系列的API。hook的开启控制是线程粒度的，可以自由选择。
//  通过hook模块，可以使一些不具异步功能的API，展现出异步的性能，
//  hook将API封装成一个与原始系统调用同名的接口，
//  在调用这个接口时，先实现一些别的操作，然后在调用原始的系统API。
//  这样对开发者来说很方便，不用重新学习新的接口，用着同步的接口实现异步的操作。
namespace cc{

bool is_hook_enable();
void set_hook_enable(bool flag);

}

extern "C"{

//sleep_fun 函数指针
//sleep_fun 指向一个函数，该函数接受一个 unsigned int 类型的参数，
//并返回一个 unsigned int 类型的值
typedef unsigned int (*sleep_fun)(unsigned int seconds);
//sleep_f 它是一个sleep_fun类型的函数指针变量，
//表示该变量在其他文件中已经定义，我们只是在当前文件中引用它
extern sleep_fun sleep_f;

//useconds_t 微秒数
typedef int (*usleep_fun)(useconds_t usec);
extern usleep_fun usleep_f;

typedef int (*nanosleep_fun)(const struct timespec *req, struct timespec *rem);
extern nanosleep_fun nanosleep_f;

//socket
typedef int (*socket_fun)(int domain, int type, int protocol);
extern socket_fun socket_f;

typedef int (*connect_fun)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern connect_fun connect_f;

typedef int (*accept_fun)(int s, struct sockaddr *addr, socklen_t *addrlen);
extern accept_fun accept_f;

//read

typedef ssize_t (*read_fun)(int fd, void *buf, size_t count);
extern read_fun read_f;

typedef ssize_t (*readv_fun)(int fd, const struct iovec *iov, int iovcnt);
extern readv_fun readv_f;

typedef ssize_t (*recv_fun)(int sockfd, void *buf, size_t len, int flags);
extern recv_fun recv_f;

typedef ssize_t (*recvfrom_fun)(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
extern recvfrom_fun recvfrom_f;

typedef ssize_t (*recvmsg_fun)(int sockfd, struct msghdr *msg, int flags);
extern recvmsg_fun recvmsg_f;

//write
typedef ssize_t (*write_fun)(int fd, const void *buf, size_t count);
extern write_fun write_f;

typedef ssize_t (*writev_fun)(int fd, const struct iovec *iov, int iovcnt);
extern writev_fun writev_f;

typedef ssize_t (*send_fun)(int s, const void *msg, size_t len, int flags);
extern send_fun send_f;

typedef ssize_t (*sendto_fun)(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
extern sendto_fun sendto_f;

typedef ssize_t (*sendmsg_fun)(int s, const struct msghdr *msg, int flags);
extern sendmsg_fun sendmsg_f;

typedef int (*close_fun)(int fd);
extern close_fun close_f;


//
typedef int (*fcntl_fun)(int fd, int cmd, ... /* arg */ );
extern fcntl_fun fcntl_f;

typedef int (*ioctl_fun)(int d, unsigned long int request, ...);
extern ioctl_fun ioctl_f;

typedef int (*getsockopt_fun)(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
extern getsockopt_fun getsockopt_f;

typedef int (*setsockopt_fun)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
extern setsockopt_fun setsockopt_f;

extern int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms);

}


#endif
