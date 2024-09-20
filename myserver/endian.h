#ifndef __CC_ENDIAN_H__
#define __CC_ENDIAN_H__

//小端存储(低位低地址)
#define CC_LITTLE_ENDIAN 1
#define CC_BIG_ENDIAN 2

#include <byteswap.h>
#include <stdint.h>
#include <type_traits>

namespace cc{

// enable_if()是 C++ 标准库中的一个工具，
// 它通常用于模板编程，主要功能是根据某些条件来启用或禁用模板特化。
// std::enable_if 可以帮助我们实现条件编译，
// 即只有在满足特定条件时，才会实例化模板函数或模板类。

//仅当T = uint64_t时，该模板才会启用
//如果第一个参数(bool 类型)为 true，std::enable_if 定义一个类型别名 type，
//并且该类型别名为第二个模板参数 T
template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type
byteswap(T value){
    return (T)bswap_64((uint64_t)value);
}

template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type
byteswap(T value){
    return (T)bswap_32((uint64_t)value);
}

template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type
byteswap(T value){
    return (T)bswap_16((uint16_t)value);
}

#if BYTE_ORDER == BIG_ENDIAN
#define CC_BYTE_ORDER CC_BIG_ENDIAN
#else
#define CC_BYTE_ORDER CC_LITTLE_ENDIAN
#endif

//结合程序编译运行环境决定是否转换
#if CC_BYTE_ORDER == CC_BIG_ENDIAN
template<class T>
T byteswapOnLittleEndian(T t){
    return t;
}

template<class T>
T byteswapOnBigEndian(T t){
    return byteswap(t);
}

#else

template<class T>
T byteswapOnLittleEndian(T t){
    return byteswap(t);
}

template<class T>
T byteswapOnBigEndian(T t){
    return t;
}


#endif


}

#endif