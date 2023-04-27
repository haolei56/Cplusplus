//
// Created by HL on 2023/4/27.
//
#include "threadpool.h"

Threadpool::Threadpool() : _threadNum(1), _bTerminate(false) {}

Threadpool::~Threadpool() { stop(); }

bool Threadpool::init(size_t num) {
    unique_lock<mutex> lock(_mutex);
    if (!_threads.empty()) {
        return false;
    }
    _threadNum = num;
    return true;
}

void Threadpool::stop() {
    {
        unique_lock<mutex> lock(_mutex);
        _bTerminate = true;
        _condition.notify_all();
    }
    for (size_t i = 0; i < _threads.size(); i++) {
        if (_threads[i]->joinable()) {
            _threads[i]->join();
        }
        delete _threads[i];
        _threads[i] = NULL;
    }
    unique_lock<mutex> lock(_mutex);
    _threads.clear();
}

bool Threadpool::start() {
    unique_lock<mutex> lock(_mutex);
    if (!_threads.empty()) {
        return false;
    }
    for (size_t i = 0; i < _threadNum; i++) {
        _threads.push_back(new thread(&Threadpool::run, this));
    }
    return true;
}

bool Threadpool::get(TaskFuncPtr &task) {
    unique_lock<mutex> lock(_mutex);
    if (_tasks.empty()) {
        _condition.wait(lock, [this] { return _bTerminate || !_tasks.empty(); });
    }
    if (_bTerminate) {
        return false;
    }
    if (!_tasks.empty()) {
        task = move(_tasks.front());
        _tasks.pop();
        return true;
    }
    return false;
}

void Threadpool::run() {
    while (!isTerminate()) {
        TaskFuncPtr task;
        bool ok = get(task);
        if (ok) {
            ++_atomic;
            try {
                if (task->_expireTime != 0 && task->_expireTime < TNOWMS) {

                } else {
                    task->_func();
                }
            }
            catch (...) {}
            --_atomic;
            unique_lock<mutex> lock(_mutex);
            if (_atomic == 0 && _tasks.empty()) {
                _condition.notify_all();
            }
        }
    }
}

bool Threadpool::waitForAllDone(int millsecond) {
    unique_lock<mutex> lock(_mutex);
    if (_tasks.empty()) {
        return true;
    }
    if (millsecond < 0) {
        _condition.wait(lock, [this] { return _tasks.empty(); });
        return true;
    } else {
        return _condition.wait_for(lock, chrono::milliseconds(millsecond), [this] { return _tasks.empty(); });
    }
}

int gettimeofday(struct timeval &tv) {
#if WIN32
    time_t clock;
    struct tm tm;
    SYSTEMTIME wtm;
    GetLocalTime(&wtm);
    tm.tm_year = wtm.wYear - 1900;
    tm.tm_mon = wtm.wMonth - 1;
    tm.tm_mday = wtm.wDay;
    tm.tm_hour = wtm.wHour;
    tm.tm_min = wtm.wMinute;
    tm.tm_sec = wtm.wSecond;
    tm.tm_isdst = -1;
    clock = mktime(&tm);
    tv.tv_sec = clock;
    tv.tv_usec = wtm.wMilliseconds * 1000;
    return 0;
#else
    return ::gettimeofday(&tv,0);
#endif
}

void getNow(timeval *tv) {
#if TARGET_PLATFORM_IOS || TARGET_PLATFORM_LINUX
    int idx=_buf_idx;
    *tv=_t[idx];
    if(fabs(_cpu_cycle-0)<0.0001&&_use_tsc){
        addTimeoffset(*tv,idx);
    }else{
        TC_Common::gettimeofday(*tv);
    }
#else
    gettimeofday(*tv);
#endif
}

int64_t getNowMs() {
    struct timeval tv;
    getNow(&tv);
    return tv.tv_sec * (int64_t) 1000 + tv.tv_usec / 1000;
}
