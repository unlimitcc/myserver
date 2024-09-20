#include "util.h"
#include "log.h"
#include <execinfo.h>
#include <sys/time.h>
#include "fiber.h"

namespace cc {
    
static Logger::ptr g_logger = CC_LOG_NAME("system");

pid_t GetThreadId(){
    //获取线程id时使用syscall获得唯一的线程id
    return syscall(SYS_gettid); 

}

//暂时未定义，输出默认0
uint64_t GetFiberId(){
    
    return cc::Fiber::GetFiberId();
}

void BackTrace(std::vector<std::string>& bt, int size, int skip){
    void **array = (void**) malloc((sizeof(void*) * size));
    //array: 一个指针数组，用于存储调用栈中的地址。
    //size: 数组的大小，即最多能够存储的地址个数。
    size_t s = ::backtrace(array, size);

    //将调用栈地址转换为可读符号信息
    char ** strings = backtrace_symbols(array,s);
    if(strings == NULL){
        CC_LOG_ERROR(g_logger) << "backtrace_symbols error";
        free(array);
        return;
    }

    for(size_t i = skip; i < s; ++i){
        bt.push_back(strings[i]);
    }
    free(array);
    free(strings);
}

//const 修饰的左值引用可以绑定到右值
std::string BackTraceToString(int size, int skip, const std::string &prefix){
    std::vector<std::string> bt;
    BackTrace(bt,size,skip);
    std::stringstream ss;
    for(size_t i = 0; i < bt.size(); ++i){
        ss << prefix << bt[i] << std::endl;
    }
    return ss.str();
}

//毫秒
uint64_t GetCurrentMS(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
}

//微妙
uint64_t GetCurrentUS(){
    struct timeval tv;
    //tv 指向一个 timeval 结构体的指针，用于存储获取到的当前时间
    //NULL 用于指向时区(可以忽略)
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 * 1000ul + tv.tv_usec;
}
} // namespace cc
