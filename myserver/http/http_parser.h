/**
 * @file http_parser.h
 * HTTP协议解析封装
 */
#ifndef __CC_HTTP_PARSER_H__
#define __CC_HTTP_PARSER_H__

#include "http.h"
#include "http11_parser.h"
#include "httpclient_parser.h"

namespace cc {
namespace http {

class HttpRequestParser{
public:
    using ptr = std::shared_ptr<HttpRequestParser>;

    /**
     * 构造函数
     */
    HttpRequestParser();

    /**
     * 解析头部协议
     * data 协议文本内存
     * len 协议文本内存长度
     * return 返回实际解析的长度,并且将已解析的数据移除
     */
    size_t execute(char* data, size_t lenk);
    //
    int isFinished();
    int hasError();
    void setError(int v) { m_error = v;}

    uint64_t getContentLength();
    HttpRequest::ptr getData() const {return m_data;}

    const http_parser& getParser() const {return m_parser;}
public:

    static uint64_t GetHttpRequestBufferSize();
    static uint64_t GetHttpRequestMaxBodySize();

private:

    http_parser m_parser;
    //HttpRequest报文
    HttpRequest::ptr m_data;
    
    //1000 -> INVALID METHOD
    //1001 -> INVALID VERSION
    //1002 -> INVALID FIELD
    int m_error;
};


class HttpResponseParser{

public:
    using ptr = std::shared_ptr<HttpResponseParser>;

    
    HttpResponseParser();
    /**
     * 解析HTTP响应协议
     * data 协议数据内存
     * len 协议数据内存大小
     * chunck 是否在解析chunck
     * 返回实际解析的长度,并且移除已解析的数据
     */
    size_t execute(char* data, size_t len, bool chunck);
    int isFinished();
    int hasError();
    void setError(int v) { m_error = v;}

    //获取消息体长度
    uint64_t getContentLength();

    HttpResponse::ptr getData() const {return m_data;}
    const httpclient_parser& getParser() const {return m_parser;}

public:
    static uint64_t GetHttpResponseBufferSize();
    static uint64_t GetHttpResponseMaxBodySize();

private:
    httpclient_parser m_parser;
    HttpResponse::ptr m_data;
    int m_error;
};

}
}

#endif
