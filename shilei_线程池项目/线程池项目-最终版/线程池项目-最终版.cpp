// 线程池项目-最终版.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <future>
#include <functional>
#include <thread>
#include "threadpool.h"
#include <chrono>

using namespace std;

int sum1(int a, int b){
    this_thread::sleep_for(std::chrono::seconds(2));
    return (a + b);
}

int main()
{
    ThreadPool pool;
    pool.setMode(PoolMode::CACHE_MODE);
    pool.start(2);
    std::future<int> res = pool.submitTask(sum1, 1, 2);
    std::future<int> res1 = pool.submitTask([](int a, int b)->int {
        int sum = 0;
        for (int i = a; i <= b; i++) {
            sum += i;
        }
        return sum;
        }, 1, 100);
    std::future<int> res2 = pool.submitTask(sum1, 2, 3);
    std::future<int> res3 = pool.submitTask(sum1, 3, 4);
    std::future<int> res4 = pool.submitTask(sum1, 4, 5);
    cout << res.get() << endl;
    cout << res1.get() << endl;
    cout << res2.get() << endl;
    cout << res3.get() << endl;
    cout << res4.get() << endl;

}

