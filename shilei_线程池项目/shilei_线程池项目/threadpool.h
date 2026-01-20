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

//c++17有写好的Any类型，接收返回的任意类型
//手写的Any，在c++14下使用
class Any {
public:
	Any() = default;
	~Any() = default;
	//unique,没有左值引用和左值赋值，成员变量是unique_ptr，所以删去左值的操作
	Any& operator=(const Any&) = delete;
	Any(const Any&) = delete;
	Any& operator=(Any&&) = default;
	Any(Any&&) = default;

	//这个构造函数能接收任意类型的数据
	template<typename T>
	Any(T data) : base_(std::make_unique<Drive<T>>(data)) {}

	//这个函数可以将Any对象中的data数据提取出来
	template<typename T>
	//从base_找到指向的Drive对象,提取出data
	T cast_() {
		//除指针、引用外，任何其他类型（值类型、void、整数、模板实例等）都不能用 dynamic_cast。
		Drive<T>* pd = dynamic_cast<Drive<T> *>(base_.get());//base_.get()可以拿到裸指针，可以进行转换。
		if (pd == nullptr) {
			throw "type is unmatch!";
		}
		return pd->data_;
	}
private:
	//基类类型
	class Base {
	public:
		virtual ~Base() = default;
	private:

	};
	//派生类类型，模板类
	template<typename T>
	class Drive : public Base {
	public:
		Drive(T data) :data_(data) {}
		T data_;
	};
private:
	//定义一个基类指针
	std::unique_ptr<Base> base_;
};

//实现一个信号量
class Semaphore {
public:
	Semaphore(int limit = 0): resLimit_(limit){}
	~Semaphore() = default;
	//获取一个信号量资源
	void wait() {
		std::unique_lock<std::mutex> lock(mtx_);
		//等待一个信号量资源，若没有资源，将阻塞当前线程
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0;});
		resLimit_--;
	}

	//增加一个信号量资源
	void post() {
		std::lock_guard<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

//提前声明
class Task;

//实现接收提交到任务队列的task执行完的返回值类型Result
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;
	
	//问题一：setVal()方法，获取任务的返回值，信号量post
	void setVal(Any any);
	//问题二：get（）方法，用户调用这个方法获取任务的返回值  信号量通信
	//wait信号量可以，就可以获得返回值，否则阻塞
	Any get();
private:
	Any any_;//存储任务返回值
	Semaphore sema_;//线程通信信号量
	std::shared_ptr<Task> task_;//指向对应的获取返回值的任务对象
	std::atomic_bool isValid_;//返回值是否有效
};
//任务抽象基类
class Task {
public:
	Task();
	~Task() = default;
	//用户自定义任务类型，从Task继承，重写run方法，实现自定义处理
	virtual Any run() = 0;

	void exec();

	void setResult(Result* result);
private:
	Result* result_;//使用裸指针，因为在Result中使用了shared_ptr<Task>，在此再使用shared_ptr会出现循环引用，内存泄漏
};

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
	Thread(ThreadFunc func);
	//线程析构函数
	~Thread();
	//线程启动
	void start();
	//当前线程id
	int getId() const;
	
private:
	ThreadFunc func_;
	static int generateId_;//不同的编号设置，静态变量需在类外初始化
	int threadId_;//保存线程id
};

//线程池类型
class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();

	//设置任务队列的上限阈值
	void setTaskQueMaxThreshHold(int threshHold);

	//设置cached模式下线程池线程最大阈值
	void setThreadSizeThreshHold(int threshold);

	//提交任务
	Result submitTask(std::shared_ptr<Task> sp);

	//设置线程池的工作模式
	void setMode(PoolMode mode);

	//启动线程池
	void start(int initThreadSize = std::thread::hardware_concurrency());//初始数量为计算机的cpu核心数量

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool operator=(const ThreadPool&) = delete;
private:
	//定义线程函数
	void ThreadFunc(int threadId);

	//检查线程池的工作状态函数
	bool checkRunningState() const;
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

