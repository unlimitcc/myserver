#ifndef __CC_BYTEARRAY_H__
#define __CC_BYTEARRAY_H__

#include <string>
#include <vector>
#include <memory>
#include <sys/types.h>
#include <sys/socket.h>
#include "endian.h"


namespace cc
{

//字节数组容器，提供基础类型的序列化与反序列化功能。
//  ByteArray的底层存储是固定大小的块，以链表形式组织。
//  每次写入数据时，将数据写入到链表最后一个块中，
//  如果最后一个块不足以容纳数据，则分配一个新的块并添加到链表结尾，
//  再写入数据。ByteArray会记录当前的操作位置，每次写入数据时，
//  该操作位置按写入大小往后偏移，如果要读取数据，则必须调用setPosition重新设置当前的操作位置。
//bytearray支持一下类型序列化和反序列化
//  1.固定长度的有符号/无符号8位、16位、32位、64位整数
//  2.不固定长度的有符号/无符号32位、64位整数
//  3.float、double类型
//  4.字符串，包含字符串长度，长度范围支持16位、32位、64位。
//  5.字符串，不包含长度。
class ByteArray{

public:

    using ptr = std::shared_ptr<ByteArray>;
    struct Node{
        Node(size_t s);
        Node();
        ~Node();

        // 内存块地址指针
        char* ptr;
        //下一块区域的地址
        Node* next;
        size_t size;
    };

    ByteArray(size_t base_size = 4096);
    ~ByteArray();

    //write
    // 写入固定长度int8_t类型的数据
    // m_position += sizeof(value)
    // 如果m_position > m_size 则 m_size = m_position
    void writeFint8(int8_t value);
    void writeFuint8(uint8_t value);
    void writeFint16(int16_t value);
    void writeFuint16(uint16_t value);
    void writeFint32(int32_t value);
    void writeFuint32(uint32_t value);
    void writeFint64(int64_t value);
    void writeFuint64(uint64_t value);

    void writeInt32(int32_t value);
    void writeUint32(uint32_t value);
    void writeInt64(int64_t value);
    void writeUint64(uint64_t value);

    void writeFloat(float value);
    void writeDouble(double value);
    
    /**
     * 写入std::string类型的数据,用uint16_t作为长度类型
     * m_position += 2 + value.size()
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeStringF16(const std::string& value);
    void writeStringF32(const std::string& value);
    void writeStringF64(const std::string& value);
    void writeStringVint(const std::string& value);

    void writeStringWithoutLength(const std::string& value);


    /**
     * 读取int8_t类型的数据
     * getReadSize() >= sizeof(int8_t)
     * m_position += sizeof(int8_t);
     * 如果getReadSize() < sizeof(int8_t) 抛出 std::out_of_range
     */
    int8_t   readFint8();

    /**
     * 读取uint8_t类型的数据
     * getReadSize() >= sizeof(uint8_t)
     * m_position += sizeof(uint8_t);
     * 如果getReadSize() < sizeof(uint8_t) 抛出 std::out_of_range
     */
    uint8_t  readFuint8();

    /**
     * 读取int16_t类型的数据
     * getReadSize() >= sizeof(int16_t)
     * m_position += sizeof(int16_t);
     * 如果getReadSize() < sizeof(int16_t) 抛出 std::out_of_range
     */
    int16_t  readFint16();

    /**
     * 读取uint16_t类型的数据
     * getReadSize() >= sizeof(uint16_t)
     * m_position += sizeof(uint16_t);
     * 如果getReadSize() < sizeof(uint16_t) 抛出 std::out_of_range
     */
    uint16_t readFuint16();

    /**
     * 读取int32_t类型的数据
     * getReadSize() >= sizeof(int32_t)
     * m_position += sizeof(int32_t);
     * 如果getReadSize() < sizeof(int32_t) 抛出 std::out_of_range
     */
    int32_t  readFint32();

    /**
     * 读取uint32_t类型的数据
     * getReadSize() >= sizeof(uint32_t)
     * m_position += sizeof(uint32_t);
     * 如果getReadSize() < sizeof(uint32_t) 抛出 std::out_of_range
     */
    uint32_t readFuint32();

    /**
     * 读取int64_t类型的数据
     * getReadSize() >= sizeof(int64_t)
     * m_position += sizeof(int64_t);
     * 如果getReadSize() < sizeof(int64_t) 抛出 std::out_of_range
     */
    int64_t  readFint64();

    /**
     * 读取uint64_t类型的数据
     * getReadSize() >= sizeof(uint64_t)
     * m_position += sizeof(uint64_t);
     * 如果getReadSize() < sizeof(uint64_t) 抛出 std::out_of_range
     */
    uint64_t readFuint64();

    /**
     * 读取有符号Varint32类型的数据
     * getReadSize() >= 有符号Varint32实际占用内存
     * m_position += 有符号Varint32实际占用内存
     * 如果getReadSize() < 有符号Varint32实际占用内存 抛出 std::out_of_range
     */
    int32_t  readInt32();

    /**
     * 读取无符号Varint32类型的数据
     * getReadSize() >= 无符号Varint32实际占用内存
     * m_position += 无符号Varint32实际占用内存
     * 如果getReadSize() < 无符号Varint32实际占用内存 抛出 std::out_of_range
     */
    uint32_t readUint32();

    /**
     * 读取有符号Varint64类型的数据
     * getReadSize() >= 有符号Varint64实际占用内存
     * m_position += 有符号Varint64实际占用内存
     * 如果getReadSize() < 有符号Varint64实际占用内存 抛出 std::out_of_range
     */
    int64_t  readInt64();

    /**
     * 读取无符号Varint64类型的数据
     * getReadSize() >= 无符号Varint64实际占用内存
     * m_position += 无符号Varint64实际占用内存
     * 如果getReadSize() < 无符号Varint64实际占用内存 抛出 std::out_of_range
     */
    uint64_t readUint64();

    /**
     * 读取float类型的数据
     * getReadSize() >= sizeof(float)
     * m_position += sizeof(float);
     * 如果getReadSize() < sizeof(float) 抛出 std::out_of_range
     */
    float    readFloat();

    /**
     * 读取double类型的数据
     * getReadSize() >= sizeof(double)
     * m_position += sizeof(double);
     * 如果getReadSize() < sizeof(double) 抛出 std::out_of_range
     */
    double   readDouble();

    /**
     * 读取std::string类型的数据,用uint16_t作为长度
     * getReadSize() >= sizeof(uint16_t) + size
     * m_position += sizeof(uint16_t) + size;
     * 如果getReadSize() < sizeof(uint16_t) + size 抛出 std::out_of_range
     */
    std::string readStringF16();

    /**
     * 读取std::string类型的数据,用uint32_t作为长度
     * getReadSize() >= sizeof(uint32_t) + size
     * m_position += sizeof(uint32_t) + size;
     * 如果getReadSize() < sizeof(uint32_t) + size 抛出 std::out_of_range
     */
    std::string readStringF32();

    /**
     * 读取std::string类型的数据,用uint64_t作为长度
     * getReadSize() >= sizeof(uint64_t) + size
     * m_position += sizeof(uint64_t) + size;
     * 如果getReadSize() < sizeof(uint64_t) + size 抛出 std::out_of_range
     */
    std::string readStringF64();

    /**
     * 读取std::string类型的数据,用无符号Varint64作为长度
     * getReadSize() >= 无符号Varint64实际大小 + size
     * m_position += 无符号Varint64实际大小 + size;
     * 如果getReadSize() < 无符号Varint64实际大小 + size 抛出 std::out_of_range
     */
    std::string readStringVint();

    /**
     * 清空ByteArray
     * m_position = 0, m_size = 0
     */
    void clear();

    /**
     * 写入size长度的数据
     * buf 内存缓存指针
     * size 数据大小
     * m_position += size, 如果m_position > m_size 则 m_size = m_position
     */
    void write(const void* buf, size_t size);

    /**
     * 读取size长度的数据
     * buf 内存缓存指针
     * size 数据大小
     * m_position += size, 如果m_position > m_size 则 m_size = m_position
     * 如果getReadSize() < size 则抛出 std::out_of_range
     */
    void read(void* buf, size_t size);

    
    size_t getPosition() const {return m_position;}

    /**
     * 设置ByteArray当前位置
     * 如果m_position > m_size 则 m_size = m_position
     * 如果m_position > m_capacity 则抛出 std::out_of_range
     */
    void setPosition(size_t v);

    /**
     * 把ByteArray的数据写入到文件中
     * name 文件名
     */
    bool writeToFile(const std::string& name) const;

    /**
     * 从文件中读取数据
     * name 文件名
     */
    bool readFromFile(const std::string& name);

    /**
     * 读取size长度的数据
     * buf 内存缓存指针
     * size 数据大小
     * position 读取开始位置
     * 如果 (m_size - position) < size 则抛出 std::out_of_range
     */
    void read(void* buf, size_t size, size_t position) const;

    /**
     * 返回内存块的大小
     */
    size_t getBaseSize() const { return m_baseSize;}

    /**
     * 返回可读取数据大小
     */
    size_t getReadSize() const { return m_size - m_position;}


    bool isLittleEndian() const;
    //设置是否为小端
    void setIsLittleEndian(bool val);

    std::string toString() const;
    std::string toHexString() const;

     /**
     * 获取可读取的缓存,保存成iovec数组
     * buffers 保存可读取数据的iovec数组
     * len 读取数据的长度,如果len > getReadSize() 则 len = getReadSize()
     * 返回实际数据的长度
     */
    /*
    struct iovec {
        void *iov_base;   // 指向缓冲区的指针
        size_t iov_len;   // 缓冲区的长度（字节数）
    };
    */
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len = ~0ull) const;

    /**
     * 获取可读取的缓存,保存成iovec数组,从position位置开始
     * buffers 保存可读取数据的iovec数组
     * len 读取数据的长度,如果len > getReadSize() 则 len = getReadSize()
     * position 读取数据的位置
     * 返回实际数据的长度
     */
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const;

    /**
     * 获取可写入的缓存,保存成iovec数组
     * buffers 保存可写入的内存的iovec数组
     * len 写入的长度
     * 返回实际的长度
     * 如果(m_position + len) > m_capacity 则 m_capacity扩容N个节点以容纳len长度
     */
    uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);

    size_t getSize() const {return m_size;}
private:

    //扩容，使其可以容纳size个数据(如果原本可以可以容纳,则不扩容)
    void addCapacity(size_t size);
    size_t getCapacity() const {return m_capacity - m_position;}

    //基本大小
    size_t m_baseSize;
    //当前操作位置
    size_t m_position;
    //当前总容量
    size_t m_capacity;
    //当前数据的大小
    size_t m_size;
    //字节序(一般默认为大端)大端
    int8_t m_endian;
    //链表第一个内存块指针
    Node* m_root;
    //当前操作的内存块指针
    Node* m_cur;
};
    

} // namespace cc


#endif