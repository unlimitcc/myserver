#include "bytearray.h"
#include "log.h"
#include <string.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <math.h>
#include <stdexcept>


namespace cc{

static cc::Logger::ptr g_logger = CC_LOG_NAME("system");

ByteArray::Node::Node(size_t s)
        :ptr(new char[s])
        ,next(nullptr)
        ,size(s){

}

ByteArray::Node::Node()
        :ptr(nullptr)
        ,next(nullptr)
        ,size(0){

}

ByteArray::Node::~Node(){
    if(ptr){
        delete[] ptr;
    }
}

ByteArray::ByteArray(size_t base_size)
        :m_baseSize(base_size)
        ,m_position(0)
        ,m_capacity(base_size)
        ,m_size(0)
        ,m_endian(CC_BIG_ENDIAN)
        ,m_root(new Node(base_size))
        ,m_cur(m_root){

}

//清除链表全部节点
ByteArray::~ByteArray(){
    Node* tmp = m_root;
    while (tmp)
    {
        m_cur = tmp;
        tmp = tmp->next;
        delete m_cur;
    }
}

bool ByteArray::isLittleEndian() const{
    return m_endian == CC_LITTLE_ENDIAN;
}

//val = true 表示小端
void ByteArray::setIsLittleEndian(bool val){
    if(val){
        m_endian = CC_LITTLE_ENDIAN;
    }else{
        m_endian = CC_BIG_ENDIAN;
    }
}

//write
void ByteArray::writeFint8(int8_t value){
    write(&value, sizeof(value));
}

void ByteArray::writeFuint8(uint8_t value){
    write(&value, sizeof(value));
}

void ByteArray::writeFint16(int16_t value){
    if(m_endian != CC_BYTE_ORDER){
        value = byteswap(value);
    }
    write(&value, sizeof(value));
    
}

void ByteArray::writeFuint16(uint16_t value){
    if(m_endian !=CC_BYTE_ORDER){
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFint32(int32_t value){
    if(m_endian !=CC_BYTE_ORDER){
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFuint32(uint32_t value){
    if(m_endian !=CC_BYTE_ORDER){
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFint64(int64_t value){
    if(m_endian !=CC_BYTE_ORDER){
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFuint64(uint64_t value){
    if(m_endian !=CC_BYTE_ORDER){
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

//将负数转为无符号数，降低数据压缩大小
//负数将符号位
// -3 -> 5, -2 -> 3, -1 -> 1, 0 -> 0, 1 -> 2, 2-> 4
//  奇数为负数
//  和1异或相当于取反操作
//  和0异或相当于不变操作
//  编码时和1异或，去除前导1
//  正数解码右移一位和全0异或，得到原数据
//  负数解码右移一位和全1异或，得到原数据
// zigzag原公式 Zigzag(n) = (n << 1) ^ (n >> 31)
// 解码
static uint32_t EncodeZigzag32(const int32_t& v){
    if(v < 0){
        return ((uint32_t)(-v)*2 - 1);
    }else{
        return v*2;
    }
}

//如果原数是负数，-(v&1)得到全1，异或得到原数据
static uint32_t DecodeZigzag32(const uint32_t& v){
    return (v >> 1) ^ -(v & 1); 
}

//7bit压缩数据
void ByteArray::writeInt32(int32_t value){
    writeUint32(EncodeZigzag32(value));
}

static uint64_t EncodeZigzag64(const int64_t& v){
    if(v < 0){
        return ((uint64_t)(-v)*2 - 1);
    }else{
        return v*2;
    }
}

//(v & 1)拿到v的最后一位，若为1则为负数
static uint64_t DecodeZigzag64(const uint64_t& v){
    return (v >> 1) ^ -(v & 1);
}

void ByteArray::writeInt64(int64_t value){
    writeUint64(EncodeZigzag64(value));
}

//Varint编码
void ByteArray::writeUint64(uint64_t value){
    uint8_t tmp[10];
    uint8_t i = 0;
    while(value >= 0x80){
        tmp[i++] = (value & 0x7f) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);
}

void ByteArray::writeFloat(float value){
    uint32_t v;
    memcpy(&v, &value, sizeof(value));
    writeFuint32(v);
}

void ByteArray::writeDouble(double value){
    uint64_t v;
    memcpy(&v, &value, sizeof(value));
    writeFuint64(v);
}

//长度: 16, data
void ByteArray::writeStringF16(const std::string& value) {
    writeFuint16(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::writeStringF32(const std::string& value) {
    writeFuint32(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::writeStringF64(const std::string& value) {
    writeFuint64(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::writeStringVint(const std::string& value) {
    writeUint64(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::writeStringWithoutLength(const std::string& value) {
    write(value.c_str(), value.size());
}

int8_t   ByteArray::readFint8() {
    int8_t v;
    read(&v, sizeof(v));
    return v;
}

uint8_t  ByteArray::readFuint8() {
    uint8_t v;
    read(&v, sizeof(v));
    return v;
}

#define XX(type) \
    type v; \
    read(&v, sizeof(v)); \
    if(m_endian == CC_BYTE_ORDER) { \
        return v; \
    } else { \
        return byteswap(v); \
    }
//byteswap即是按字节为单位进行交换
int16_t  ByteArray::readFint16() {
    XX(int16_t);
}

uint16_t ByteArray::readFuint16() {
    XX(uint16_t);
}

int32_t  ByteArray::readFint32() {
    XX(int32_t);
}

uint32_t ByteArray::readFuint32() {
    XX(uint32_t);
}

int64_t  ByteArray::readFint64() {
    XX(int64_t);
}

uint64_t ByteArray::readFuint64() {
    XX(uint64_t);
}

#undef XX

int32_t  ByteArray::readInt32() {
    return DecodeZigzag32(readUint32());
}

int64_t  ByteArray::readInt64() {
    return DecodeZigzag64(readUint64());
}



void ByteArray::writeUint32 (uint32_t value) {
    uint8_t tmp[5];
    uint8_t i = 0;
    while(value >= 0x80) {
        tmp[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);
}

//反解出压缩的数据
uint32_t ByteArray::readUint32() {
    uint32_t result = 0;
    for(int i = 0; i < 32; i += 7) {
        uint8_t b = readFuint8();
        if(b < 0x80) {
            result |= ((uint32_t)b) << i;
            break;
        } else {
            result |= (((uint32_t)(b & 0x7f)) << i);
        }
    }
    return result;
}

uint64_t ByteArray::readUint64() {
    uint64_t result = 0;
    for(int i = 0; i < 64; i += 7) {
        uint8_t b = readFuint8();
        if(b < 0x80) {
            result |= ((uint64_t)b) << i;
            break;
        } else {
            result |= (((uint64_t)(b & 0x7f)) << i);
        }
    }
    return result;
}

float    ByteArray::readFloat() {
    uint32_t v = readFuint32();
    float value;
    memcpy(&value, &v, sizeof(v));
    return value;
}

double   ByteArray::readDouble() {
    uint64_t v = readFuint64();
    double value;
    memcpy(&value, &v, sizeof(v));
    return value;
}

std::string ByteArray::readStringF16() {
    uint16_t len = readFuint16();
    std::string buff;
    buff.resize(len);
    read(&buff[0], len);
    return buff;
}

std::string ByteArray::readStringF32() {
    uint32_t len = readFuint32();
    std::string buff;
    buff.resize(len);
    read(&buff[0], len);
    return buff;
}

std::string ByteArray::readStringF64() {
    uint64_t len = readFuint64();
    std::string buff;
    buff.resize(len);
    read(&buff[0], len);
    return buff;
}

std::string ByteArray::readStringVint() {
    uint64_t len = readUint64();
    std::string buff;
    buff.resize(len);
    read(&buff[0], len);
    return buff;
}

void ByteArray::clear() {
    m_position = m_size = 0;
    m_capacity = m_baseSize;
    Node* tmp = m_root->next;
    while(tmp) {
        m_cur = tmp;
        tmp = tmp->next;
        delete m_cur;
    }
    m_cur = m_root;
    m_root->next = NULL;
}
//buffer中的内容写到链表中，并调整当前指针位置(即m_position的位置)
void ByteArray::write(const void* buf, size_t size) {
    if(size == 0) {
        return;
    }
    addCapacity(size);

    //当前写的位置相对于页头部的偏移量 
    size_t npos = m_position % m_baseSize;
    //当前页剩余容量
    size_t ncap = m_cur->size - npos;
    //待写缓冲区的位置
    size_t bpos = 0;

    while(size > 0) {
        //当前页内可以写下来
        if(ncap >= size) {
            memcpy(m_cur->ptr + npos, (const char*)buf + bpos, size);
            //这一页满了
            if(m_cur->size == (npos + size)){
                m_cur = m_cur->next;
            }
            m_position += size;
            bpos += size;
            size = 0;
        } else {
            //当前页写不下，先写满，在换页
            memcpy(m_cur->ptr + npos, (const char*)buf + bpos, ncap);
            m_position += ncap;
            bpos += ncap;
            size -= ncap;
            m_cur = m_cur->next;
            ncap = m_cur->size;
            npos = 0;
        }
    }

    if(m_position > m_size) {
        m_size = m_position;
    }
}

//链表内容读到buffer中，并调整当前指针位置
void ByteArray::read(void* buf, size_t size) {
    if(size > getReadSize()) {
        throw std::out_of_range("not enough len");
    }
    
    //当前页开始读取位置
    size_t npos = m_position % m_baseSize;
    //当前页的剩余容量
    size_t ncap = m_cur->size - npos;
    //当前已读
    size_t bpos = 0;
    while(size > 0) {
        //当前页包含了全部的读数据
        if(ncap >= size) {
            memcpy((char*)buf + bpos, m_cur->ptr + npos, size);
            if(m_cur->size == (npos + size)) {
                m_cur = m_cur->next;
            }
            m_position += size;
            bpos += size;
            size = 0;
        } else {
            //当前页不够读，先读完再读下一页
            memcpy((char*)buf + bpos, m_cur->ptr + npos, ncap);
            m_position += ncap;
            bpos += ncap;
            size -= ncap;
            m_cur = m_cur->next;
            ncap = m_cur->size;
            npos = 0;
        }
    }
}
//指定位置开始读，不改变链表m_position
void ByteArray::read(void* buf, size_t size, size_t position) const {
    if(size > getReadSize()) {
        throw std::out_of_range("not enough len");
    }

    size_t npos = position % m_baseSize;
    //当前页的剩余容量
    size_t ncap = m_cur->size - npos;
    //当前已读
    size_t bpos = 0;

    Node* cur = m_cur;
    while(size > 0){
        if(ncap >= size){
            memcpy((char*)buf + bpos, cur->ptr + npos, size);
            if(cur->size == (npos + size)) {
                cur = cur->next;
            }
            position += size;
            bpos += size;
            size = 0;
        }else{
            memcpy((char*)buf + bpos, cur->ptr + npos, ncap);
            position += ncap;
            bpos += ncap;
            size -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
    }
}

void ByteArray::setPosition(size_t v) {
    if(v > m_capacity) {
        throw std::out_of_range("set_position out of range");
    }
    m_position = v;
    if(m_position > m_size) {
        m_size = m_position;
    }
    m_cur = m_root;
    while(v > m_cur->size) {
        v -= m_cur->size;
        m_cur = m_cur->next;
    }
    if(v == m_cur->size) {
        m_cur = m_cur->next;
    }
    //调整m_cur即当前块指针的位置
}

//ByteArray写到文件中
bool ByteArray::writeToFile(const std::string& name) const {
    std::ofstream ofs;
    ofs.open(name, std::ios::trunc | std::ios::binary);
    if(!ofs) {
        CC_LOG_ERROR(g_logger) << "writeToFile name=" << name
            << " error , errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }

    int64_t read_size = getReadSize();
    int64_t pos = m_position;
    Node* cur = m_cur;

    while(read_size > 0) {
        int diff = pos % m_baseSize;
        int64_t len = (read_size > (int64_t)m_baseSize ? m_baseSize : read_size) - diff;
        // ofstream& write(const char* buffer, streamsize size);
        // buffer指向需要写入文件的内存缓冲区（字节数组）。
        ofs.write(cur->ptr + diff, len);
        cur = cur->next;
        pos += len;
        read_size -= len;
    }
    return true;
}
//从文件中读到链表中
bool ByteArray::readFromFile(const std::string& name) {
    std::ifstream ifs;
    ifs.open(name, std::ios::binary);
    if(!ifs) {
        CC_LOG_ERROR(g_logger) << "readFromFile name=" << name
            << " error, errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }

    //自定义析构函数
    std::shared_ptr<char> buff(new char[m_baseSize], [](char* ptr) { delete[] ptr;});
    while(!ifs.eof()) {
        //buff: 指向用于存储读取数据的内存缓冲区（字符数组）。
        ifs.read(buff.get(), m_baseSize);
        //gcount 返回读取的字节数，只能在某些非格式化输入操作后使用，例如 read、getline。
        //这些操作从流中读取数据，但可能并未读取所有请求的字节数。
        write(buff.get(), ifs.gcount());
    }
    return true;
}

void ByteArray::addCapacity(size_t size) {
    if(size == 0) {
        return;
    }
    size_t old_cap = getCapacity();
    if(old_cap >= size) {
        return;
    }

    //额外增加的容量的页数,按页增加
    size = size - old_cap;
    //double ceil(double x)返回大于或等于x的最小的整数值
    //count 表示需要扩充的页面数
    size_t count = ceil(1.0 * size / m_baseSize);
    Node* tmp = m_root;
    while(tmp->next) {
        tmp = tmp->next;
    }

    //记录第一个新加的页
    Node* first = NULL;
    for(size_t i = 0; i < count; ++i) {
        tmp->next = new Node(m_baseSize);
        if(first == NULL) {
            first = tmp->next;
        }
        tmp = tmp->next;
        m_capacity += m_baseSize;
    }

    //如果旧容量已经为零，指针指向新块的第一块
    if(old_cap == 0) {
        m_cur = first;
    }
}

std::string ByteArray::toString() const {
    std::string str;
    str.resize(getReadSize());
    if(str.empty()) {
        return str;
    }
    //无副作用读，读到str中
    read(&str[0], str.size(), m_position);
    return str;
}

//转为16进制字符串
std::string ByteArray::toHexString() const {
    std::string str = toString();
    std::stringstream ss;

    for(size_t i = 0; i < str.size(); ++i){
        if(i > 0 && i % 32 == 0) {
            ss << std::endl;
        }
        ss << std::setw(2) << std::setfill('0') << std::hex
           << (int)(uint8_t)str[i] << " ";
    }
    return ss.str();
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const {
    len = len > getReadSize() ? getReadSize() : len;
    if(len == 0) {
        return 0;
    }

    uint64_t size = len;

    size_t npos = m_position % m_baseSize;
    //当前块容量
    size_t ncap = m_cur->size - npos;
    struct iovec iov;
    Node* cur = m_cur;

    while(len > 0) {
        //当前块即包括剩余需要读的数据
        if(ncap >= len){
            //起始位置
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        }else{
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            //还需要读取的数据量
            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}
//获取可读取的缓存,保存成iovec数组,从position位置开始
uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers
                                ,uint64_t len, uint64_t position) const {
    len = len > getReadSize() ? getReadSize() : len;
    if(len == 0) {
        return 0;
    }

    uint64_t size = len;

    size_t npos = position % m_baseSize;
    size_t count = position / m_baseSize;
    Node* cur = m_root;
    while(count > 0) {
        cur = cur->next;
        --count;
    }

    //获取position所指的页
    size_t ncap = cur->size - npos;
    struct iovec iov;
    while(len > 0) {
        if(ncap >= len) {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}
//将数据写入buffers
uint64_t ByteArray::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len) {
    if(len == 0) {
        return 0;
    }
    addCapacity(len);
    uint64_t size = len;

    size_t npos = m_position % m_baseSize;
    size_t ncap = m_cur->size - npos;
    struct iovec iov;
    Node* cur = m_cur;
    while(len > 0) {
        if(ncap >= len){
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        }else{
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}

}