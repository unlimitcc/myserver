#ifndef __CC_UTIL_H__
#define __CC_UTIL_H__

#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <string>
#include <vector>

namespace cc {

pid_t GetThreadId();

uint64_t GetFiberId();
/**
 * 获取当前的调用栈
 * bt 保存调用栈
 * size 最多返回层数
 * skip 跳过栈顶的层数
 */
void BackTrace(std::vector<std::string>& bt, int size = 64, int skip = 1);
/**
 * 获取当前栈信息的字符串
 * size     栈的最大层数
 * skip     跳过栈顶的层数
 * prefix   栈信息前输出的内容(类似于格式)
 */
std::string BackTraceToString(int size = 64, int skip = 2, const std::string &prefix = " ");

//时间
uint64_t GetCurrentMS();
uint64_t GetCurrentUS();

}

#endif