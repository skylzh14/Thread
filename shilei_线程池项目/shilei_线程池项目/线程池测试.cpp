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
	{
	//死锁问题测试，有几率出现线程不能结束，线程在等待notEmpty，析构函数在等待exitCond，可是这两个不能被唤醒了
	// 发生在线程池析构函数将线程池状态改变之前，线程在就绪状态准备抢锁，而抢锁后，线程池状态改变。
	//1、若析构函数在析构时先抢到taskQueMtx_，然后进入exitCond_ wait,等待threads_==0，释放锁
	//线程函数从就绪态再抢到taskQueMtx_，通过双重判断线程池的运行状态，也就是说在拿到锁之后再判断一次
	//如果线程池已关闭，则跳出循环，删除threads_的这个线程，唤醒exitCond_。
	//2、若线程先抢到taskQueMtx_，进入notEmpty，等待任务列表不空，释放锁
	//析构函数再抢到taskQueMtx_，这时应该唤醒等待线程，notEmpty_ notify,线程便会发现线程池结束了
	//则跳出循环，删除threads_的这个线程，唤醒exitCond_。
	ThreadPool pool;
	pool.setMode(PoolMode::CACHE_MODE);
	pool.start(2);
	Result res1 = pool.submitTask(std::make_shared<MyTask2>(1, 1000));
	pool.submitTask(std::make_shared<MyTask2>(1, 1000));
	pool.submitTask(std::make_shared<MyTask2>(1, 1000));
	pool.submitTask(std::make_shared<MyTask2>(1, 1000));
	pool.submitTask(std::make_shared<MyTask2>(1, 1000));
	int sum1 = res1.get().cast_<int>();
	std::cout << "sum:" << sum1 << std::endl;
	
	}

	std::cout << "main thread is over!" << std::endl;
if (0)
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