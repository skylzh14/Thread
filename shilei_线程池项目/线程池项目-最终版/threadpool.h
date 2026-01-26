#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <future>

#define TASK_MAX_THRESHHOLD 1024//避免魔鬼数字
#define THREAD_MAX_THRESHHOLD 10
#define	THREAD_MAX_IDLE_TIME 10//单位 s

//线程池模式
enum class PoolMode {
	FIXED_MODE,		//固定数量线程
	CACHE_MODE,		//线程数量可动态增长
};

//线程类型
class Thread {
public:
	//线程函数对象 类型
	using ThreadFunc = std::function<void(int)>;
	//线程构造函数
	Thread(ThreadFunc func) :
		func_(func), threadId_(generateId_++) {

	}
	//线程析构函数
	~Thread();
	//线程启动
	void start() {
		//创建一个线程执行一个线程函数
		std::thread t(func_, threadId_);//线程对象 t 和线程函数func
		t.detach();//分离线程
	}
	//当前线程id
	int getId() const {
		return threadId_;
	}

private:
	ThreadFunc func_;
	static int generateId_;//不同的编号设置，静态变量需在类外初始化
	int threadId_;//保存线程id
};

int Thread::generateId_ = 0;

//线程池类型
class ThreadPool {
public:
	ThreadPool():initThreadSize_(4), taskSize_(0),
		taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
		poolMode_(PoolMode::FIXED_MODE),
		isPoolRunning_(false),
		idleThreadSize_(0),
		threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
		curThreadSize_(0) {
	};
	~ThreadPool() {
		isPoolRunning_ = false;

		//等待线程池里面的线程返回，阻塞或者正在执行任务
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		notEmpty_.notify_all();//唤醒所有阻塞线程
		exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0;});
	}

	//设置任务队列的上限阈值
	void setTaskQueMaxThreshHold(int threshHold) {
		if (checkRunningState()) return;
		taskQueMaxThreshHold_ = threshHold;
	}

	//设置cached模式下线程池线程最大阈值
	void setThreadSizeThreshHold(int threshold) {
		if (checkRunningState()) return;
		if (poolMode_ == PoolMode::CACHE_MODE)
			threadSizeThreshHold_ = threshold;
	}

	//提交任务
	Result submitTask(std::shared_ptr<Task> sp) {
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
	void setMode(PoolMode mode) {
		if (checkRunningState()) return;
		poolMode_ = mode;
	}

	//启动线程池
	void start(int initThreadSize = std::thread::hardware_concurrency())//初始数量为计算机的cpu核心数量
	{
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

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool operator=(const ThreadPool&) = delete;
private:
	//定义线程函数
	void ThreadFunc(int threadId) {  //线程函数返回，线程结束
		//线程需要将任务队列的所有任务执行完，才能析构，不能用isPoolRunning_作为判断条件
		for (;;) {
			std::shared_ptr<Task> task;
			auto lastTime = std::chrono::high_resolution_clock().now();
			{	//获取锁
				std::unique_lock<std::mutex> lock(taskQueMtx_);
				std::cout << "tid:" << std::this_thread::get_id() << "try to get mutex!" << std::endl;
				//若有任务，不进行循环，直接拿任务执行
				while (taskQue_.size() == 0) {
					//当任务为0时，看线程池状态，决定是否删除线程
					if (!isPoolRunning_) {
						threads_.erase(threadId);
						std::cout << "threadId:" << std::this_thread::get_id() << "exit!" << std::endl;
						exitCond_.notify_all();
						return;
					}
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
	}

	//检查线程池的工作状态函数
	bool checkRunningState() const {
		return isPoolRunning_;
	}

private:
	//std::vector<std::unique_ptr<Thread>> threads_;//线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;//线程列表,记录线程id
	size_t initThreadSize_; //线程初始数量
	std::atomic_int idleThreadSize_; //空闲线程的数量
	int threadSizeThreshHold_;//线程数量上线阈值
	std::atomic_int curThreadSize_;//当前线程池中线程的数量

	std::queue<std::shared_ptr<Task>> taskQue_;//任务队列
	std::atomic_int taskSize_;//任务数量
	size_t taskQueMaxThreshHold_;//任务队列的最大阈值

	std::mutex taskQueMtx_; //保证任务队列线程安全
	std::condition_variable notFull_;	//表示任务队列不满
	std::condition_variable notEmpty_;	//表示任务队列不空
	std::condition_variable exitCond_;  //表示线程退出

	PoolMode poolMode_; //当前线程池的工作模式
	std::atomic_bool isPoolRunning_;//当前线程池的状态

};

#endif // !THREADPOOL_H

