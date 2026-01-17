#include <iostream>
#include <chrono>

#include "threadpool.h"

class MyTask : public Task {
public:
	Any run() {
		std::cout << "tid:" << std::this_thread::get_id() << "begin!" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		std::cout << "tid:" << std::this_thread::get_id() << "end!" << std::endl;
	}
};

class MyTask1 : public Task {
public:
	Any run() {
		std::cout << "tid::" << std::this_thread::get_id() << "开始执行MyTask1" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(2));
		std::cout << "tid::" << std::this_thread::get_id() << "睡2秒后,MyTask1结束" << std::endl;
	}
};

class MyTask2 : public Task {
public:
	MyTask2(int begin, int end):begin_(begin), end_(end){}
	Any run() {
		std::cout << "tid::" << std::this_thread::get_id() << "开始执行MyTask2" << std::endl;
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
	ThreadPool pool;
	pool.start(4);

	Result res = pool.submitTask(std::make_shared<MyTask2>());
	int sum = res.get().cast_<int>//类型由用户提供

	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask1>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask1>());

	getchar();
	return 0;
}