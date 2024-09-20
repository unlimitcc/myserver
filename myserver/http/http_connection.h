 #ifndef __CC_HTTP_CONNECTION_H__
#define __CC_HTTP_CONNECTION_H__

//http客户端
#include <memory>
#include "../socket_stream.h"
#include "http.h"
#include "http_parser.h"
#include "../uri.h"
#include "../thread.h"
#include <list>

namespace cc{

namespace http{

/**
 * HTTP响应结果
 */
struct HttpResult {
    
    using ptr = std::shared_ptr<HttpResult>;
    /**
     * 错误码定义
     */
    enum class Error {
        // 正常
        OK = 0,
        // 非法URL
        INVALID_URL = 1,
        // 无法解析HOST
        INVALID_HOST = 2,
        // 连接失败
        CONNECT_FAIL = 3,
        // 连接被对端关闭
        SEND_CLOSE_BY_PEER = 4,
        // 发送请求产生Socket错误
        SEND_SOCKET_ERROR = 5,
        // 超时
        TIMEOUT = 6,
        // 创建Socket失败
        CREATE_SOCKET_ERROR = 7,
        // 从连接池中取连接失败
        POOL_GET_CONNECTION = 8,
        // 无效的连接
        POOL_INVALID_CONNECTION = 9,
    };

    /**
     * 构造函数
     * _result 错误码
     * _response HTTP响应结构体
     * _error 错误描述
     */
    HttpResult(int _result
               ,HttpResponse::ptr _response
               ,const std::string& _error)
        :result(_result)
        ,response(_response)
        ,error(_error) {}

    /// 错误码
    int result;
    /// HTTP响应结构体
    HttpResponse::ptr response;
    /// 错误描述
    std::string error;

    std::string toString() const;
};


class HttpConnection : public SocketStream {
friend class HttpConnectionPool;
public:
    using ptr = std::shared_ptr<HttpConnection>;

    /**
     * 发送HTTP的GET请求
     * url 请求的url
     * timeout_ms 超时时间(毫秒)
     * headers HTTP请求头部参数
     * body 请求消息体
     * 返回HTTP结果结构体
     */
    static HttpResult::ptr DoGet(const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");

    /**
     * 发送HTTP的GET请求
     * uri URI结构体
     * timeout_ms 超时时间(毫秒)
     * headers HTTP请求头部参数
     * body 请求消息体
     * 返回HTTP结果结构体
     */
    static HttpResult::ptr DoGet(Uri::ptr uri
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");

    /**
     * 发送HTTP的POST请求
     * url 请求的url
     * timeout_ms 超时时间(毫秒)
     * headers HTTP请求头部参数
     * body 请求消息体
     * 返回HTTP结果结构体
     */
    static HttpResult::ptr DoPost(const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");

    /**
     * 发送HTTP的POST请求
     * uri URI结构体
     * timeout_ms 超时时间(毫秒)
     * headers HTTP请求头部参数
     * body 请求消息体
     * 返回HTTP结果结构体
     */
    static HttpResult::ptr DoPost(Uri::ptr uri
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");

    //下面的支持任意类型的请求
    /**
     * 发送HTTP请求
     * method 请求类型
     * uri 请求的url
     * timeout_ms 超时时间(毫秒)
     * headers HTTP请求头部参数
     * body 请求消息体
     * 返回HTTP结果结构体
     */
    static HttpResult::ptr DoRequest(HttpMethod method //(method : GET/POST...)
                            , const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");

    /**
     * 发送HTTP请求
     * method 请求类型
     * uri URI结构体
     * timeout_ms 超时时间(毫秒)
     * headers HTTP请求头部参数
     * body 请求消息体
     * return 返回HTTP结果结构体
     */
    static HttpResult::ptr DoRequest(HttpMethod method
                            , Uri::ptr uri
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");

    /**
     * 发送HTTP请求
     * req 请求结构体
     * uri URI结构体
     * timeout_ms 超时时间(毫秒)
     * return 返回HTTP结果结构体
     * 已经有request结构体，只是负责转发
     */
    static HttpResult::ptr DoRequest(HttpRequest::ptr req
                            , Uri::ptr uri
                            , uint64_t timeout_ms);

    /**
     * 构造函数
     * sock Socket类
     * owner 是否掌握所有权
     */
    HttpConnection(Socket::ptr sock, bool owner = true);

    /**
     * 析构函数
     */
    ~HttpConnection();

    /**
     * 接收HTTP响应
     */
    HttpResponse::ptr recvResponse();

    /**
     * 发送HTTP请求
     * req HTTP请求结构
     */
    int sendRequest(HttpRequest::ptr req);

private:
    //创建时间
    uint64_t m_createTime = 0;
    //请求次数
    uint64_t m_request = 0;
};

class HttpConnectionPool {
public:
    using ptr = std::shared_ptr<HttpConnectionPool>;
    using MutexType = Mutex;

    static HttpConnectionPool::ptr Create(const std::string& uri
                                        ,const std::string& vhost
                                        ,uint32_t max_size
                                        ,uint32_t max_alive_time
                                        ,uint32_t max_request);

    HttpConnectionPool(const std::string& host
                      ,const std::string& vhost
                      ,uint32_t port
                      ,bool is_https
                      ,uint32_t max_size
                      ,uint32_t max_alive_time
                      ,uint32_t max_request);

    // 取连接
    HttpConnection::ptr getConnection();

    /**
     * 发送HTTP的GET请求
     * url 请求的url
     * timeout_ms 超时时间(毫秒)
     * headers HTTP请求头部参数
     * body 请求消息体
     * return 返回HTTP结果结构体
     */
    HttpResult::ptr doGet(const std::string& url
                          , uint64_t timeout_ms
                          , const std::map<std::string, std::string>& headers = {}
                          , const std::string& body = "");

    /**
     * 发送HTTP的GET请求
     * uri URI结构体
     * timeout_ms 超时时间(毫秒)
     * headers HTTP请求头部参数
     * body 请求消息体
     * 返回HTTP结果结构体
     */
    HttpResult::ptr doGet(Uri::ptr uri
                           , uint64_t timeout_ms
                           , const std::map<std::string, std::string>& headers = {}
                           , const std::string& body = "");

    /**
     * 发送HTTP的POST请求
     * url 请求的url
     * timeout_ms 超时时间(毫秒)
     * headers HTTP请求头部参数
     * body 请求消息体
     * 返回HTTP结果结构体
     */
    HttpResult::ptr doPost(const std::string& url
                           , uint64_t timeout_ms
                           , const std::map<std::string, std::string>& headers = {}
                           , const std::string& body = "");

    /**
     * 发送HTTP的POST请求
     * uri URI结构体
     * timeout_ms 超时时间(毫秒)
     * headers HTTP请求头部参数
     * body 请求消息体
     * 返回HTTP结果结构体
     */
    HttpResult::ptr doPost(Uri::ptr uri
                           , uint64_t timeout_ms
                           , const std::map<std::string, std::string>& headers = {}
                           , const std::string& body = "");

    /**
     * 发送HTTP请求
     * method 请求类型
     * uri 请求的url
     * timeout_ms 超时时间(毫秒)
     * headers HTTP请求头部参数
     * body 请求消息体
     * 返回HTTP结果结构体
     */
    HttpResult::ptr doRequest(HttpMethod method
                            , const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");

    /**
     * 发送HTTP请求
     * method 请求类型
     * uri URI结构体
     * timeout_ms 超时时间(毫秒)
     * headers HTTP请求头部参数
     * body 请求消息体
     * 返回HTTP结果结构体
     */
    HttpResult::ptr doRequest(HttpMethod method
                            , Uri::ptr uri
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");

    /**
     * 发送HTTP请求
     * req 请求结构体
     * timeout_ms 超时时间(毫秒)
     * 返回HTTP结果结构体
     */
    HttpResult::ptr doRequest(HttpRequest::ptr req
                            , uint64_t timeout_ms);
private:
    static void ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool);
private:

    //主机
    std::string m_host;
    std::string m_vhost;
    //端口号
    uint32_t m_port;
    //连接池上限
    uint32_t m_maxSize;
    //最大存活时间
    uint32_t m_maxAliveTime;
    //每条连接最多处理多少条连接
    uint32_t m_maxRequest;
    //是否https
    bool m_isHttps;

    MutexType m_mutex;

    std::list<HttpConnection*> m_conns;
    //连接数量
    std::atomic<int32_t> m_total = {0};
};


}
}


#endif