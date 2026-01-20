#include <iostream>
#include <chrono>

#include "threadpool.h"

class MyTask : public Task {
public:
	Any run() {
		std::cout << "tid:" << std::this_thread::get_id() << "begin!" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		std::cout << "tid:" << std::this_thread::get_id() << "end!" << std::endl;
		return "";
	}
};

class MyTask1 : public Task {
public:
	Any run() {
		std::cout << "tid::" << std::this_thread::get_id() << "开始执行MyTask1" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(2));
		std::cout << "tid::" << std::this_thread::get_id() << "睡2秒后,MyTask1结束" << std::endl;
		return "";
	}
};

class MyTask2 : public Task {
public:
	MyTask2(int begin, int end):begin_(begin), end_(end){}
	//c++11不能使用auto作为函数的返回值类型，必须给出确定的
	//run方法最终在线程池分配的线程中执行
	Any run() {
		std::cout << "tid::" << std::this_thread::get_id() << "开始执行MyTask2" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		int sum = 0;
		for (int i = begin_; i < end_; ++i) {
			sum += i;
		}
		return sum;
		std::cout << "tid::" << std::this_thread::get_id() << "MyTask2结束" << std::endl;
	}
private:
	int begin_;
	int end_;
};

int main() {
	//线程池ThreadPool对象析构，线程池相关资源回收
	{
		ThreadPool pool;
		//用户设置自己的线程池的模式
		pool.setMode(PoolMode::CACHE_MODE);
		pool.start(4);//start 后，就不允许setMode，所以需要对pool的状态做一个记录

		//Master-Slave模型
		//Master线程负责拆分任务到各个Slave线程
		//等到Slave线程执行完返回结果
		//Master合并各个Slave线程的结果
		Result res1 = pool.submitTask(std::make_shared<MyTask2>(1, 1000));
		Result res2 = pool.submitTask(std::make_shared<MyTask2>(1001, 2000));
		Result res3 = pool.submitTask(std::make_shared<MyTask2>(2001, 3000));


		pool.submitTask(std::make_shared<MyTask2>(1001, 2000));
		pool.submitTask(std::make_shared<MyTask2>(2001, 3000));
		pool.submitTask(std::make_shared<MyTask2>(2001, 3000));

		int sum1 = res1.get().cast_<int>();//res1.get().cast_<int>() 类型由用户提供
		int sum2 = res2.get().cast_<int>();
		int sum3 = res3.get().cast_<int>();
		std::cout << "1-3000累加的结果是：" << sum1 + sum2 + sum3 << std::endl;
	}
	
	
	



	/*pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask1>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask1>());*/

	getchar();
	return 0;
}