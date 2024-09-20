#include "http_session.h"
#include "http_parser.h"
#include <sstream>

namespace cc {
namespace http {

HttpSession::HttpSession(Socket::ptr sock, bool owner)
    :SocketStream(sock, owner) {
}

HttpRequest::ptr HttpSession::recvRequest() {
    HttpRequestParser::ptr parser(new HttpRequestParser);
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    // uint64_t buff_size = 10;
    std::shared_ptr<char> buffer(new char[buff_size], [](char* ptr){
                delete[] ptr;
            });
    char* data = buffer.get();
    int offset = 0;
    do {
        //接受缓冲区data要往前移动一些，因为原缓冲区可能有暂时未解析的函数
        //len表示一次读取的数据
        //buff_size - offset表示buff实际可用的容量
        int len = read(data + offset, buff_size - offset);
        if(len <= 0) {
            std::cout<< "len is error" << std::endl;
            //close();
            return nullptr;
        }
        //之前还有剩的数据加到本次待解析的内容中
        len += offset;

        //解析刚才读取的数据，返回已经解析过的数据
        size_t nparse = parser->execute(data, len);
        if(parser->hasError()) {
            close();
            return nullptr;
        }
        
        //一次性未解析完剩的数据
        offset = len - nparse;
        //如果缓冲区满了且本次没有处理任何数据
        if(offset == (int)buff_size) {
            close();
            return nullptr;
        }
        if(parser->isFinished()) {
            break;
        }
    } while(true);

    // 获取HTTP请求的内容长度，body
    int64_t length = parser->getContentLength();
    if(length > 0) {
        std::string body;
        body.resize(length);

        int len = 0;
        //此时有数据不在data中
        if(length >= offset) {
            memcpy(&body[0], data, offset);
            len = offset;
        } else {
            memcpy(&body[0], data, length);
            len = length;
        }
        length -= offset;
        if(length > 0) {
            if(readFixSize(&body[len], length) <= 0) {
                close();
                return nullptr;
            }
        }
        parser->getData()->setBody(body);
    }

    //parser->getData()->init();
    return parser->getData();
}

int HttpSession::sendResponse(HttpResponse::ptr rsp) {
    std::stringstream ss;
    ss << *rsp;
    std::string data = ss.str();
    return writeFixSize(data.c_str(), data.size());
}

}
}
