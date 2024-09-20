#ifndef __THREAD_H__
#define __THREAD_H__
//线程只做为协程的承载容器,使用pthread_create进行互斥量实现，读写分离
//C++-11 before pthread
//C++-11 after std::thread

#include <thread>
#include <functional>
#include <memory>
#include <string>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <atomic>
#include "noncopyable.h"

namespace cc{

//Noncopyable: 不可被复制，删掉了对应的拷贝构造函数并删除了重载的赋值"="运算符
//为什么封装：
//C++11的thread里面没有提供读写互斥量，RWMutex，Spinlock等，
//在高并发场景，这些对象是经常需要用到的，所以选择自己封装pthread。
//信号量
class Semaphore : Noncopyable{
public:
    Semaphore(uint32_t count = 0);
    ~Semaphore();

    void wait();
    void notify();
private:
    sem_t m_semaphore;
};

//局部锁模板
//通过构造析构实现上锁和解锁
template<class T>
struct ScopedLockImpl{
public:
    ScopedLockImpl(T& mutex)
        :m_mutex(mutex){
        m_mutex.lock();
        m_locked = true;
    }

    ~ScopedLockImpl(){
        unlock();
    }

    void lock(){
        if(!m_locked){
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock(){
        if(m_locked){
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked;
};

//局部读锁模板
template<class T>
struct ReadScopedLockImpl{
public:
    ReadScopedLockImpl(T& mutex)
        :m_mutex(mutex){
        m_mutex.rdlock();
        m_locked = true;
    }

    ~ReadScopedLockImpl(){
        unlock();
    }

    void lock(){
        if(!m_locked){
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    void unlock(){
        if(m_locked){
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked;
};

//局部写锁
template<class T>
struct WriteScopedLockImpl{
public:
    WriteScopedLockImpl(T& mutex)
        :m_mutex(mutex){
        m_mutex.wrlock();
        m_locked = true;
    }

    ~WriteScopedLockImpl(){
        unlock();
    }

    void lock(){
        if(!m_locked){
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    void unlock(){
        if(m_locked){
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked;
};

//预备知识
//int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
//  一个用于初始化 POSIX 线程库中的互斥锁的函数。返回 0 表示互斥锁初始化成功,
//  attr 表示 1个可选的指向 pthread_mutexattr_t 类型的指针，用于指定互斥锁的属性,默认属性传递NULL。
//pthread_rwlock_destroy 
//  释放读写锁: 注意<此函数只是反初始化读写锁变量，并没有释放内存空间，如果读写锁变量是通过malloc等函数申请的，
//  那么需要在free掉读写锁变量之前调用pthread_rwlock_destory函数>
//当一个线程调用pthread_mutex_unlock()时，该互斥锁将被标记为未被持有，
//并且如果有其它线程正在等待该锁，则其中一个线程将被唤醒以继续执行。
class Mutex : Noncopyable{

public:
    using Lock = ScopedLockImpl<Mutex>;
    Mutex(){
        pthread_mutex_init(&m_mutex, nullptr);
    }
    ~Mutex(){
        pthread_mutex_destroy(&m_mutex);
    }
    void lock(){
        pthread_mutex_lock(&m_mutex);
    }
    void unlock(){
        pthread_mutex_unlock(&m_mutex);
    }
private:
    pthread_mutex_t m_mutex;
};

//调试使用
class NullMutex : Noncopyable{
public:
    using Lock = ScopedLockImpl<NullMutex>;
    NullMutex() {}
    ~NullMutex() {}
    void lock() {}
    void unlock() {}
private:

};

//读写分离互斥锁
//pthread_rwlock_init与pthread_lock_init类似，但是支持读写分离
//与互斥锁不同，读写锁允许多个线程同时读取共享资源，但只允许一个线程写入共享资源。
//这样可以提高程序的性能和效率，但需要注意避免读写锁死锁等问题。
class RWMutex : Noncopyable{
public:

    using ReadLock = ReadScopedLockImpl<RWMutex>;
    using WriteLock = WriteScopedLockImpl<RWMutex>;

    RWMutex(){
        pthread_rwlock_init(&m_lock, nullptr);
    }

    ~RWMutex(){
        pthread_rwlock_destroy(&m_lock);
    }
    //读写锁的读 / 写 加锁方式不同
    //有线程已经持有写入锁，则其他线程将被阻塞直到写入锁被释放。
    //调用此函数时，如果另一个线程已经持有写入锁，则该线程将被阻塞，直到写入锁被释放。
    void rdlock(){
        pthread_rwlock_rdlock(&m_lock);
    }
    //有其他线程已经持有读取或写入锁，则调用此函数的线程将被阻塞，直到所有的读取和写入锁都被释放。
    void wrlock(){
        pthread_rwlock_wrlock(&m_lock);
    }

    void unlock(){
        pthread_rwlock_unlock(&m_lock);
    }
private:
    pthread_rwlock_t m_lock;

};

//调试使用,测试加锁和不加锁的区别
class NullRWMutex : Noncopyable{
public:
    using ReadLock = ReadScopedLockImpl<NullMutex>;
    using WriteLock = WriteScopedLockImpl<NullMutex>;
    NullRWMutex() {}
    ~NullRWMutex() {}
    void rdlock() {}
    void wrlock() {}
    void unlock() {}
private:
};

//与mutex不同，自旋锁不会使线程进入睡眠状态，而是在获取锁时进行忙等待，直到锁可用。
//当锁被释放时，等待获取锁的线程将立即获取锁，从而避免了线程进入和退出睡眠状态的额外开销。
class Spinlock : Noncopyable{
public:
    using Lock = ScopedLockImpl<Spinlock>;
    Spinlock(){
        pthread_spin_init(&m_mutex, 0);
    }
    ~Spinlock(){
        pthread_spin_destroy(&m_mutex);
    }
    void lock(){
        pthread_spin_lock(&m_mutex);
    }

    void unlock(){
        pthread_spin_unlock(&m_mutex);
    }
private:
    pthread_spinlock_t m_mutex;

};

//原子锁
class CASLock : Noncopyable{

public:
    using Lock = ScopedLockImpl<CASLock>;
    CASLock(){
        //atomic_flag.clear()可以轻松地重置标志位，使之再次可用于控制对共享资源的访问
        m_mutex.clear();
    }

    ~CASLock(){

    }

    void lock(){
        //是 C++ 标准库中的一个原子操作函数，用于对 std::atomic_flag 进行操作
        //返回原子操作执行前 std::atomic_flag 的状态，
        //如果之前为true，则返回true，继续自旋，直到设置成功
        //std::memory_order_acquire: 确保后续的读写操作不会被重排序到此操作之前
        //所有该线程之前发生的写操作都被完全同步到主内存中
        while(std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire));
    }

    void unlock(){
        //确保之前的读写操作不会被重排序到此操作之后
        std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
    }
private:

    volatile std::atomic_flag m_mutex;
};

class Thread : Noncopyable{

public:
    using ptr = std::shared_ptr<Thread>;
    
    //线程函数，线程名称
    Thread(std::function<void()> cb, const std::string &name);
    ~Thread();

    //获取线程ID
    pid_t getId() {return m_id;}
    //获取线程名称
    const std::string& getName() const {return m_name;}

    //阻塞等待线程完成
    void join();
    //获取当前的线程指针
    static Thread* GetThis();

    //用于日志->获取自身当前使用的线程名称
    static const std::string& GetName(); 
    //设置线程名称
    static void SetName(const std::string& name);
private:

    //线程执行函数
    static void* run(void* arg);
private:

    //线程id
    pid_t m_id = -1;
    //线程标识符
    pthread_t m_thread = 0;
    //线程执行函数
    std::function<void()> m_cb;
    //线程名称
    std::string m_name;
    //线程中使用的信号量
    Semaphore m_semaphore;
};

}




#endif