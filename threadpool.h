//
// Created by HL on 2023/4/25.
//

#ifndef MY_THREAD_POOL_THREADPOOL_H
#define MY_THREAD_POOL_THREADPOOL_H
#include <future>
#include <functional>
#include <iostream>
#include <queue>
#include <mingw.mutex.h>
#include <mingw.condition_variable.h>
#include <mingw.thread.h>
#include <mingw.future.h>
#include <memory>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
using namespace std;

void getNow(timeval *tv);
int64_t getNowMs();

#define TNOW getNow()
#define TNOWMS getNowMs()
/////////////////////////////////////////////////
/**
* @file thread_pool.h
* @brief 线程池类,采用c++11来实现了，
* 使用说明:
* ThreadPool tpool;
* tpool.init(5); //初始化线程池线程数
* //启动线程方式
* tpool.start();
* //将任务丢到线程池中
* tpool.exec(testFunction, 10); //参数和start相同
* //等待线程池结束
* tpool.waitForAllDone(1000); //参数<0时, 表示无限等待(注意有人调用stop也会推出)
* //此时: 外部需要结束线程池是调用
* tpool.stop();
* 注意:
* ThreadPool::exec执行任务返回的是个future, 因此可以通过future异步获取结果, 比如:
* int testInt(int i)
* {
* return i;
* }
* auto f = tpool.exec(testInt, 5);
* cout << f.get() << endl; //当testInt在线程池中执行后, f.get()会返回数值5
*
* class Test
* {
* public:
* int test(int i);
* };
* Test t;
* auto f = tpool.exec(std::bind(&Test::test, &t, std::placeholders::_1), 10);
* //返回的future对象, 可以检查是否执行
* cout << f.get() << endl;
*/

class Threadpool{
protected:
    struct TaskFunc{
        TaskFunc(uint64_t expireTime):_expireTime(expireTime){}
        std::function<void()> _func;
        int64_t _expireTime=0;
    };
    typedef shared_ptr<TaskFunc> TaskFuncPtr;
public:
    /**
    * @brief 构造函数
    *
    */
    Threadpool();
    /**
    * @brief 析构, 会停止所有线程
    */
    virtual ~Threadpool();
    /**
    * @brief 初始化.
    *
    * @param num 工作线程个数
    */
    bool init(size_t num);
    /**
    * @brief 获取线程个数.
    *
    * @return size_t 线程个数
    */
    size_t getThreadNum(){
        unique_lock<mutex> lock(_mutex);
        return _threads.size();
    }
    /**
    * @brief 获取当前线程池的任务数
    *
    * @return size_t 线程池的任务数
    */
    size_t getJobNum(){
        unique_lock<std::mutex> lock(_mutex);
        return _tasks.size();
    }

protected:
    bool get(TaskFuncPtr& task);
    bool isTerminate(){return _bTerminate;}
    void run();
protected:
    queue<TaskFuncPtr> _tasks;
    vector<thread*> _threads;
    mutex _mutex;
    condition_variable _condition;
    size_t _threadNum;
    bool _bTerminate;
    atomic<int> _atomic{0};
};

#endif //MY_THREAD_POOL_THREADPOOL_H
