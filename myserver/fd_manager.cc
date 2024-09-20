#include "fd_manager.h"
#include "hook.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace cc{

FdCtx::FdCtx(int fd)
    :m_isInit(false)
    ,m_isSocket(false)
    ,m_sysNonblock(false)
    ,m_userNonblock(false)
    ,m_isClosed(false)
    ,m_fd(fd)
    ,m_recvTimeout(-1)
    ,m_sendTimeout(-1){
    init();
}

FdCtx::~FdCtx(){

}

//
bool FdCtx::init(){
    if(m_isInit){
        return true;
    }
    //默认超时时间(无超时时间)
    m_recvTimeout = -1;
    m_sendTimeout = -1;
    
    //struct stat
    //用于存储文件的元数据（即文件的状态信息）。
    //该结构体包含了许多与文件相关的信息，如文件类型、权限、大小、时间戳
    //fstat() 是一个系统调用，用于获取与文件描述符(file descriptor)关联的文件的状态信息，
    //并将这些信息存储在 struct stat 结构体中。
    struct stat fd_stat;
    if(-1 == fstat(m_fd, &fd_stat)){
        //初始化失败
        m_isInit = false;
        m_isSocket = false;
    }else{
        m_isInit = true;
        //st_mode: 文件的类型和权限
        //是否是socket
        m_isSocket = S_ISSOCK(fd_stat.st_mode);
    }

    //是否是socket
    if(m_isSocket) {
        // 获取文件的flags
        int flags = fcntl_f(m_fd, F_GETFL, 0);
        // 如果为非阻塞
        if(!(flags & O_NONBLOCK)){
            //设置为阻塞
            fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
        }
        //需要hook
        m_sysNonblock = true;
    } else {
        m_sysNonblock = false;
    }

    //初始化用户不设置为阻塞
    m_userNonblock = false;
    m_isClosed = false;
    return m_isInit;    
}

//套接字选项SO_RCVTIMEO： 用来设置socket接收数据的超时时间；
void FdCtx::setTimeout(int type, uint64_t v){
    if(type == SO_RCVTIMEO){
        m_recvTimeout = v;
    }else{
        m_sendTimeout = v;
    }
}

//获取超时时间
uint64_t FdCtx::getTimeout(int type){
    if(type == SO_RCVTIMEO){
        return m_recvTimeout;
    }else{
        return m_sendTimeout;
    }
}

//
FdManager::FdManager(){
    m_datas.resize(64);

}

//获取,根据auto_create判断是否自动创建
FdCtx::ptr FdManager::get(int fd, bool auto_create){
    if(fd == -1){
        return nullptr;
    }
    RWMutexType::ReadLock lock(m_mutex);
    //查看是否已经创建
    if((int)m_datas.size() <= fd) {
        //没有且不自动创建
        if(auto_create == false){
            return nullptr;
        }
    } else {
        //如果有或者不用自动创建
        if(m_datas[fd] || !auto_create){
            return m_datas[fd];
        }
    }
    lock.unlock();

    //没有但需要自动创建
    RWMutexType::WriteLock lock2(m_mutex);
    if (fd >= (int)m_datas.size()) {
        m_datas.resize(fd * 1.5);
    }
    FdCtx::ptr ctx(new FdCtx(fd));
    m_datas[fd] = ctx;
    return ctx;
}

void FdManager::del(int fd){
    RWMutexType::WriteLock lock(m_mutex);
    //没有找到
    if((int)m_datas.size() <= fd) {
        return;
    } 
    m_datas[fd].reset();
}

}