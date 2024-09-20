#ifndef __CC_HTTP_SESSION_H__
#define __CC_HTTP_SESSION_H__

#include <memory>
#include "../socket_stream.h"
#include "http.h"
#include "http_parser.h"

//服务器端建立好连接的socket 对应一个 session
//功能: 处理请求，发送响应
//该类继承于SocketStream，用于接收请求报文，发送响应报文。
namespace cc{

namespace http{
class HttpSession : public SocketStream {
public:
    using ptr = std::shared_ptr<HttpSession>;

    /**
     * 构造函数
     * Socket类型
     * owner 表示在析构时是否需要程序自定中断连接。
     */
    HttpSession(Socket::ptr sock, bool owner = true);

    // 接收HTTP请求
    HttpRequest::ptr recvRequest();

    // function: 发送HTTP响应
    // rsp HTTP响应
    // return >0 发送成功
    //        =0 对方关闭
    //        <0 Socket异常
    int sendResponse(HttpResponse::ptr rsp);
};    
}



}


#endif