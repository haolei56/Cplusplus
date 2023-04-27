//
// Created by HL on 2023/4/25.
//

#ifndef MY_THREAD_POOL_THREADPOOL_H
#define MY_THREAD_POOL_THREADPOOL_H

#include <future>
#include <functional>
#include <iostream>
#include <queue>
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

class Threadpool {
protected:
    struct TaskFunc {
        TaskFunc(uint64_t expireTime) : _expireTime(expireTime) {}
        std::function<void()> _func;
        int64_t _expireTime = 0;
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
    * @param num 工作线程个数
    */
    bool init(size_t num);

    /**
    * @brief 获取线程个数.
    * @return size_t 线程个数
    */
    size_t getThreadNum() {
        unique_lock<mutex> lock(_mutex);
        return _threads.size();
    }

    /**
    * @brief 获取当前线程池的任务数
    * @return size_t 线程池的任务数
    */
    size_t getJobNum() {
        unique_lock<mutex> lock(_mutex);
        return _tasks.size();
    }

    /**
     * @brief 停止所有线程，会等待所有线程结束
     */
    void stop();

    /**
     * @brief启动所有线程
     */
    bool start();

    /**
     *
     * @brief 用线程池启动任务（F是function，Args是参数）
     * @tparam ParentFunctor
     *在函数体内部，它调用了一个带有参数包 args... 的递归辅助函数 exec，其中第一个参数 0 是一个计数器，用于跟踪当前执行的递归深度。
     * 函数的返回类型是 auto，根据函数 f 的返回值类型 decltype(f(args...)) 推断得出，用 std::future 包装后返回。
     * @return 返回任务的future对象，可以通过这个对象来获取返回值
     */
    template<class F, class... Args>
    auto exec(F &&f, Args &&... args) -> std::future<decltype(f(args...))> {
        return exec(0, f, args...);
    }

    /**
    * @brief 用线程池启用任务(F是function, Args是参数)
    * @param 超时时间 ，单位ms (为0时不做超时控制) ；若任务超时，此任务将被丢弃
    * @param bind function
    * @return 返回任务的future对象, 可以通过这个对象来获取返回值
    */
    /*
    template <class F, class... Args>
    它是c++里新增的最强大的特性之一，它对参数进行了高度泛化，它能表示0到任意个数、任意类型的参数
    箭头符号“->”在这里用于指定函数的返回类型。这种语法通常称为尾返回类型（trailing return type），它在C++11中被引入。尾返回类型主要用于模板函数中返回类型过于复杂，需要使用decltype等关键字才能得到的情况。
    这段代码是一个任务调度器的实现，主要功能是将一个可调用对象和一些参数封装成一个任务，并加入任务队列中，等待被执行。
    */
    template<class F, class... Args>
    auto exec(int64_t timeoutMs, F &&f, Args &&... args) -> future<decltype(f(args...))> {
        int64_t expireTime = (timeoutMs == 0 ? 0 : TNOWMS + timeoutMs);  // 获取现在时间

        using RetType = decltype(f(args...));   // using给类型定义一个别名

        auto task = make_shared<packaged_task<RetType()>>(//std::packaged_task 是一个类模板，用于封装一个可调用对象（函数、函数对象、Lambda 表达式等）并创建一个与该封装任务关联的 std::future 对象。该 std::future 对象可以获取封装任务的结果。
                bind(forward<F>(f), forward<Args>(args)...));//首先使用bind将函数f与参数args绑定起来，以便之后可以执行函数f。然后使用packaged_task将绑定后的函数f封装成一个可调用的任务对象，并返回一个指向该对象的智能指针task。

        TaskFuncPtr fptr = make_shared<TaskFunc>(expireTime);      // 封装任务指针，设置过期时间.这里定义了一个名为fptr的指向TaskFunc类型的智能指针，使用make_shared创建了一个TaskFunc对象，并将过期时间expireTime作为构造函数的参数传递进去。
        fptr->_func = [task]() {      // 具体执行的函数,这里定义了一个lambda表达式，用于具体执行任务。将指向packaged_task对象的智能指针task作为lambda表达式的捕获变量，并在lambda表达式中调用该对象的()运算符，以执行任务。最后将该lambda表达式赋值给TaskFunc对象的_func成员变量，以便之后可以调用该lambda表达式执行任务。
            (*task)();
        };
        unique_lock<mutex> lock(_mutex);
        _tasks.push(fptr);         // 插入任务
        _condition.notify_one();      // 唤醒阻塞的线程，可以考虑只有任务队列为空的情况再去notify
        return task->get_future();
    }

    /**
     * @brief 等待当前任务队列中, 所有工作全部结束(队列无任务).
     * @param millsecond 等待的时间(ms), -1:永远等待
     * @return   true, 所有工作都处理完毕; false,超时退出
     */
    bool waitForAllDone(int millsecond = -1);

protected:
    /**
     * @brief 获取任务
     * @param task
     * @return TaskFuncPtr
     */
    bool get(TaskFuncPtr &task);

    /**
     * @brief 线程池是否退出
     * @return
     */
    bool isTerminate() { return _bTerminate; }

    /**
     * @brief线程运行状态
     */
    void run();

protected:
    queue<TaskFuncPtr> _tasks;//任务队列
    vector<thread *> _threads;//工作线程
    mutex _mutex;
    condition_variable _condition;
    size_t _threadNum;
    bool _bTerminate;
    atomic<int> _atomic{0};
};

#endif //MY_THREAD_POOL_THREADPOOL_H
