#ifndef __CC_FD_MANAGER_H__
#define __CC_FD_MANAGER_H__

#include <memory>
#include "thread.h"
#include "iomanager.h"
// #include <sys/socket.h>
// #include <sys/types.h>
#include "singleton.h"
#include <vector>

//更好的管理socket文件，
//使用fdmanager管理每个socket fd，
//并且为每个socket文件隐式的设置为O_NONBLOCK非阻塞

namespace cc{
//FdCtx存储每一个fd相关的信息，并由FdManager管理每一个FdCtx，FdManager为单例类
class FdCtx : public std::enable_shared_from_this<FdCtx>{

public:
    using ptr = std::shared_ptr<FdCtx>;
    FdCtx(int fd);
    ~FdCtx();
    bool init() ;
    bool isInit()const {return m_isInit;}
    bool isSocket() const {return m_isSocket;}
    bool isClose() const {return m_isClosed;}
    bool close();

    void setUserNonBlock(bool v) {m_userNonblock = v;}
    bool getUserNonBlock() {return m_userNonblock;}

    void setSysNonBlock(bool v) {m_sysNonblock = v;}
    bool getSysNonBlock() {return m_sysNonblock;}

    void setTimeout(int type, uint64_t v);
    uint64_t getTimeout(int type);
private:
    //是否初始化
    bool m_isInit : 1;
    //是否是socket
    bool m_isSocket : 1;
    //是否 hook非阻塞
    bool m_sysNonblock : 1;
    //是否 用户主动设置非阻塞
    bool m_userNonblock : 1;
    //是否关闭
    bool m_isClosed : 1;
    //文件描述符
    int m_fd;
    //读超时时间/毫秒
    uint64_t m_recvTimeout;
    //写超时时间毫秒
    uint64_t m_sendTimeout;
    //cc::IOManager* m_iomanager;
};

class FdManager{
public:
    using RWMutexType = RWMutex;
    FdManager();

    FdCtx::ptr get(int fd, bool auto_create = false);
    void del(int fd);
private:
    RWMutexType m_mutex;
    std::vector<FdCtx::ptr> m_datas;

};

typedef Singleton<FdManager> FdMgr;

}

#endif