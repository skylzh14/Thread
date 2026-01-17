#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

//c++17有写好的Any类型，接收返回的任意类型
//手写的Any，在c++14下使用
class Any {
public:
	Any() = default;
	~Any() = default;
	//unique,没有左值引用和左值赋值，成员变量是unique_ptr，所以删去左值的操作
	Any& operator=(const Any&) = delete;
	Any(const Any&) = delete;
	Any& operator=(const Any&&) = delete;
	Any(Any&&) = default;

	//这个构造函数能接收任意类型的数据
	template<typename T>
	Any(T data) : base_(std::make_unique<Drive<T>>(data)) {}

	//这个函数可以将Any对象中的data数据提取出来
	template<typename T>
	//从base_找到指向的Drive对象,提取出data
	T cast_() {
		//除指针、引用外，任何其他类型（值类型、void、整数、模板实例等）都不能用 dynamic_cast。
		Drive<T>* pd = dynamic_cast<Drive<T> *>(base_.get());
		if (pd == nullptr) {
			throw "type is unmatch!";
		}
		return data;
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

//任务抽象积累
class Task {
public:
	//用户自定义任务类型，从Task继承，重写run方法，实现自定义处理
	virtual Any run() = 0;
private:
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
	using ThreadFunc = std::function<void()>;
	//线程构造函数
	Thread(ThreadFunc func);
	//线程析构函数
	~Thread();
	//线程启动
	void start();
	
private:
	ThreadFunc func_;
};

//线程池类型
class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();

	//设置任务队列的上限阈值
	void setTaskQueMaxThreshHold(int threshHold);

	//提交任务
	void submitTask(std::shared_ptr<Task> sp);

	//设置线程池的工作模式
	void setMode(PoolMode mode);

	//启动线程池
	void start(int initThreadSize = 4);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool operator=(const ThreadPool&) = delete;
private:
	//定义线程函数
	void ThreadFunc();
private:
	std::vector<std::unique_ptr<Thread>> threads_;//线程列表
	size_t initThreadSize_; //线程初始数量

	std::queue<std::shared_ptr<Task>> taskQue_;//任务队列
	std::atomic_uint taskSize_;//任务数量
	size_t taskQueMaxThreshHold_;//任务队列的最大阈值

	std::mutex taskQueMtx_; //保证任务队列线程安全
	std::condition_variable notFull_;	//表示任务队列不满
	std::condition_variable notEmpty_;	//表示任务队列不空

	PoolMode poolMode_; //当前线程池的工作模式

};

#endif // !THREADPOOL_H

