#include <iostream>
#include "threadpool.h"

using namespace std;

void func0() {
    cout << "func0()" << endl;
}

void func1(int a) {
    cout << "func1() a=" << a << endl;
}

void func2(int a, string b) {
    cout << "func1() a=" << a << ",b=" << b << endl;
}

void test1() {
    Threadpool threadpool;
    threadpool.init(2);
    threadpool.start();
    threadpool.exec(1000, func0);
    threadpool.exec(func1, 10);
    threadpool.exec(func2, 20, "HL");
    threadpool.waitForAllDone();
    threadpool.stop();
}

int func1_future(int a) {
    cout << "func1() a=" << a << endl;
    return a;
}

string func2_future(int a, string b) {
    cout << "func1() a=" << a << ",b=" << b << endl;
    return b;
}

void test2() {
    Threadpool threadpool;
    threadpool.init(2);
    threadpool.start();
    future<decltype(func1_future(0))> result1 = threadpool.exec(func1_future, 10);
    future<string> result2 = threadpool.exec(func2_future, 20, "HL");
    cout << "result1: " << result1.get() << endl;
    cout << "result2: " << result2.get() << endl;
    threadpool.waitForAllDone();
    threadpool.stop();
}

int main() {
    test1();
    cout << "Hello World!" << endl;
    return 0;
}

