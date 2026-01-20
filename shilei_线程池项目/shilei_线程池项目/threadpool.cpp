#include "threadpool.h"
#define TASK_MAX_THRESHHOLD 1024//避免魔鬼数字
#define THREAD_MAX_THRESHHOLD 10
#define	THREAD_MAX_IDLE_TIME 10//单位 s
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
ThreadPool::ThreadPool() :
	initThreadSize_(4), taskSize_(0),
	taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
	poolMode_(PoolMode::FIXED_MODE),
	isPoolRunning_(false),
	idleThreadSize_(0),
	threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
	curThreadSize_(0)
{ }

//线程池析构
ThreadPool::~ThreadPool() {
	isPoolRunning_ = false;
	
	//等待线程池里面的线程返回，阻塞或者正在执行任务
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();//唤醒所有阻塞线程
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0;});

}

//检查线程池的工作状态函数
bool ThreadPool::checkRunningState() const {
	return isPoolRunning_;
}

//设置cached模式下线程池线程最大阈值
void ThreadPool::setThreadSizeThreshHold(int threshold) {
	if (checkRunningState()) return;
	if (poolMode_ == PoolMode::CACHE_MODE) 
		threadSizeThreshHold_ = threshold;
}

//设置任务队列的上限阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshHold) {
	if (checkRunningState()) return;
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
	std::cout << "放入一个任务" << std::endl;

	//通知notEmpty_,消费任务
	notEmpty_.notify_all();

	//Cached模式 任务处理比较紧急，场景：小而快的任务类型
	//需要根据任务数量和空闲线程数量，判断是否需要增加线程
	if (poolMode_ == PoolMode::CACHE_MODE
		&& curThreadSize_ < threadSizeThreshHold_
		&& taskSize_ > idleThreadSize_) {
		std::cout << "create a new thread..." << std::endl;
		//创建线程对象时，把线程函数给thread线程对象,ThreadFunc传入一个参数（线程id）
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::ThreadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		/*threads_.emplace_back(std::move(ptr));*/
		//启动线程
		threads_[threadId]->start();
		//相关的成员变量的更改
		curThreadSize_++;
		idleThreadSize_++;
	}

	return Result(sp, true);
}

//设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode) {
	if (checkRunningState()) return;
	poolMode_ = mode;
}

//启动线程池
void ThreadPool::start(int initThreadSize) {
	//记录线程个数
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;
	
	//设置线程池的工作状态
	isPoolRunning_ = true;
	//创建线程对象
	for (int i = 0; i < initThreadSize_; ++i) {
		//创建线程对象时，把线程函数给Thread线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::ThreadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		/*threads_.emplace_back(std::move(ptr));*/
	}
	//启动所有线程
	for (int i = 0; i < initThreadSize_; ++i) {
		threads_[i]->start();//去执行一个线程函数
		idleThreadSize_++;
	}
}
//定义线程函数
void ThreadPool::ThreadFunc(int threadId) {  //线程函数返回，线程结束
	/*std::cout << "begin threadfunc id:" << std::this_thread::get_id() << std::endl;
	std::cout << "end threadfunc id:" << std::this_thread::get_id() << std::endl;*/
	while(isPoolRunning_) {
		std::shared_ptr<Task> task;
		auto lastTime = std::chrono::high_resolution_clock().now();
		{	//获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << "tid:" << std::this_thread::get_id() << "try to get mutex!" << std::endl;
			//再次判断线程池状态，避免死锁
			while (isPoolRunning_ && taskQue_.size() == 0) {
			//cached模式下可能创建很多线程，如果空闲时间超过60s，则回收线程
			//回收超过initThreadSize_的线程
			//当前时间-上一次执行任务时间>60s
				if (poolMode_ == PoolMode::CACHE_MODE) {
					//每一秒返回一次  区分超时返回，有任务待返回
				
					//超时返回
					if (std::cv_status::timeout == 
						notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() > THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_) {
							//回收线程
							//记录一些变量的值
							//把线程对象从线程列表中删除
							threads_.erase(threadId);
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "threadId:" << std::this_thread::get_id() << "exit!" << std::endl;
							return;
						}
					}
				}
				else {
					//等待notEmpty_条件
					notEmpty_.wait(lock);
				}
				////线程池要结束了，线程被唤醒，则结束该线程，
				//if (!isPoolRunning_) {
				//	threads_.erase(threadId);
				//	//线程池要结束了，就可以不用维护这两个变量了
				//	/*curThreadSize_--;
				//	idleThreadSize_--;*/
				//	std::cout << "threadId:" << std::this_thread::get_id() << "exit!" << std::endl;
				//	exitCond_.notify_all();//通知退出判断，否则一直析构函数一直阻塞着。
				//	return;//函数结束，线程结束！
				//}
			}
			//优化  如果线程池关闭，就不用执行任务了，跳出循环，删除线程。
			if (!isPoolRunning_) {
				break;
			}
			

			idleThreadSize_--;//拿到任务，空闲线程-1
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
		idleThreadSize_++;//这里是线程的执行步骤，执行完任务task->exec()，才能到这一行
		lastTime = std::chrono::high_resolution_clock().now();//记录上一次执行任务的时间
	}
	//出了while循环，线程池要结束了，则结束该线程
	threads_.erase(threadId);
	std::cout << "threadId:" << std::this_thread::get_id() << "exit!" << std::endl;
	exitCond_.notify_all();
	return;
	
}
/// <summary>
/// 线程方法实现
/// </summary>

int Thread::generateId_ = 0;

//线程构造函数
Thread::Thread(ThreadFunc func):
	func_(func),threadId_(generateId_++) {
	
}
//线程析构函数
Thread::~Thread() {

}

////当前线程id
int Thread::getId() const {
	return threadId_;
}

void Thread::start() {
	//创建一个线程执行一个线程函数
	std::thread t(func_, threadId_);//线程对象 t 和线程函数func
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
	std::cout << "等待任务执行完成！" << std::endl;
	sema_.wait();
	std::cout << "已获得任务结果！" << std::endl;
	return std::move(any_);
}

void Result::setVal(Any any) {
	//存储any到any_
	any_ = std::move(any);
	std::cout << "保存任务结果！" << std::endl;
	sema_.post();//已获取任务返回值，增加信号量资源
}