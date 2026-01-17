#include "threadpool.h"
#define TASK_MAX_THRESHHOLD 1024//避免魔鬼数字
#include <functional>
#include <thread>
#include <iostream>
#include <chrono>

/*
* example:
ThreadPool pool;
pool.start(4);

class MyTask : public Task{
	void run(){
	};
};

pool.submitTask(std::make_shared<MyTask>());
*/



//线程池构造函数
ThreadPool::ThreadPool():
	initThreadSize_(4), taskSize_(0),
	taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
	poolMode_(PoolMode::FIXED_MODE)
{ }

//线程池析构
ThreadPool::~ThreadPool() {

}

//设置任务队列的上限阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshHold) {
	taskQueMaxThreshHold_ = threshHold;
}

//提交任务 用户调用该接口 传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {
	//获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	//判断任务队列是否为满，notFull_等待，释放锁
	//用户提交任务，等待一秒，如果任务队列一直为满，则输出超时信息
	//如果超过一秒，wait_for会返回false
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_;})) {
		//notFull_等待一秒钟，还未满足
		std::cerr << "task queue is full, submit task fail." << std::endl;
		return Result(sp, false);
	}

	//若不满，则将任务放到任务队列
	taskQue_.emplace(sp);
	taskSize_++;

	//通知notEmpty_,消费任务
	notEmpty_.notify_all();
	return Result(sp, true);
}

//设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode) {
	poolMode_ = mode;
}

//启动线程池
void ThreadPool::start(int initThreadSize) {
	//记录线程个数
	initThreadSize_ = initThreadSize;
	
	//创建线程对象
	for (int i = 0; i < initThreadSize_; ++i) {
		//创建线程对象时，把线程函数给thread线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::ThreadFunc, this));
		threads_.emplace_back(std::move(ptr));
	}
	//启动所有线程
	for (int i = 0; i < initThreadSize_; ++i) {
		threads_[i]->start();//去执行一个线程函数
	}
}
//定义线程函数
void ThreadPool::ThreadFunc() {
	/*std::cout << "begin threadfunc id:" << std::this_thread::get_id() << std::endl;
	std::cout << "end threadfunc id:" << std::this_thread::get_id() << std::endl;*/
	for (;;) {
		std::shared_ptr<Task> task;
		{	//获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << "tid:" << std::this_thread::get_id() << "try to get mutex!" << std::endl;
			//等待notEmpty_条件
			notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0;});
			std::cout << "tid:" << std::this_thread::get_id() << "already got mutex!" << std::endl;
			//如果taskQue_不空，则取任务
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			//如果任务队列依然有任务剩余，则通知其他线程取任务，notEmpty_
			if (taskQue_.size() > 0)
				notEmpty_.notify_all();
			//取出任务后，需要通知notFull_
			notFull_.notify_all();
		}//取到任务后，释放锁
		//当前线程负责执行这个任务
		if (task != nullptr)
			//task->run();//运行任务
			//还需要记录任务的返回值，run是一个虚函数，则用exec包含run实现。
			task->exec();
	}
}
/// <summary>
/// 线程方法实现
/// </summary>

//线程构造函数
Thread::Thread(ThreadFunc func):func_(func) {
	
}
//线程析构函数
Thread::~Thread() {

}
void Thread::start() {
	//创建一个线程执行一个线程函数
	std::thread t(func_);//线程对象 t 和线程函数func
	t.detach();//分离线程
}

/// //// Task实现
Task::Task():result_(nullptr){}

void Task::exec() {
	if(result_ != nullptr)
		result_->setVal(run());//这里发生多态的调用
}

void Task::setResult(Result* result) {
	result_ = result;
}

/// //// Result实现
Result::Result(std::shared_ptr<Task> task, bool isValid):
	task_(task),isValid_(isValid)
{
	task_->setResult(this);
}

Any Result::get() {
	if (!isValid_) {
		return "";
	}
	//task如果没有执行完，需要等待,需要将用户进程阻塞
	sema_.wait();
	return std::move(any_);
}

void Result::setVal(Any any) {
	//存储any到any_
	any_ = std::move(any);
	sema_.post();//已获取任务返回值，增加信号量资源
}