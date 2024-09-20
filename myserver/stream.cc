#include "stream.h"

namespace cc {

int Stream::readFixSize(void* buffer, size_t length) {
    size_t offset = 0;
    int64_t left = length;
    while(left > 0) {
        int64_t len = read((char*)buffer + offset, left);
        if(len <= 0) {
            return len;
        }
        offset += len;
        left -= len;
    }
    return length;
}
//读固定长度的数据，读取到ByteArray 
//ByteArray对象内部有一个位置指针，所以不需要手动更新偏移量
int Stream::readFixSize(ByteArray::ptr ba, size_t length) {
    int64_t left = length;
    while(left > 0) {
        int64_t len = read(ba, left);
        if(len <= 0) {
            return len;
        }
        left -= len;
    }
    return length;
}

//从buffer写到fd
int Stream::writeFixSize(const void* buffer, size_t length) {
    size_t offset = 0;
    int64_t left = length;
    while(left > 0) {
        int64_t len = write((const char*)buffer + offset, left);
        if(len <= 0) {
            return len;
        }
        offset += len;
        left -= len;
    }
    return length;

}

int Stream::writeFixSize(ByteArray::ptr ba, size_t length) {
    int64_t left = length;
    while(left > 0) {
        int64_t len = write(ba, left);
        if(len <= 0) {
            return len;
        }
        left -= len;
    }
    return length;
}

}
