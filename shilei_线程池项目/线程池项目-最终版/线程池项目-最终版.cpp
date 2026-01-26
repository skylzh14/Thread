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
    return (a + b) ;
}

int main()
{
    ThreadPool pool;
    pool.start(4);
    std::future<int> res = pool.submitTask(sum1, 1, 2);
    cout << res.get() << endl;
   /* packaged_task<int(int, int)> task(sum1);
    future<int> res = task.get_future();

    thread t(std::move(task), 1, 2);
    t.detach();
    
    cout << res.get() << endl;*/
}

