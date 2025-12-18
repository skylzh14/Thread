#pragma once
#include <vector>
#include <thread>
#include <functional>
#include <atomic>
#include "ThreadSafeQueue.h"
#include <future>
class ThreadPool {
private:
	std::vector<std::thread> workers_;
	ThreadSafeQueue<std::function<void()>> tasks_;
	std::atomic<bool> stop_{ false };

	void worker() { //取任务
		while (!stop_) {
			auto task = tasks_.pop();//拿到线程池中的任务函数，task就是packed_task,绑定的函数和future；pop()如果队列为空，则一直阻塞等待。
			task();//执行传进的函数。
		}
	}

public:
	ThreadPool(int num_threads) {
		for (int i = 0; i < num_threads; ++i)
			workers_.emplace_back(&ThreadPool::worker, this);//创建线程开始，根据worker(),则线程一直处于阻塞等待状态
	}

	~ThreadPool() {
		stop_ = true;
		//提交空任务唤醒阻塞进程
		for (size_t i = 0; i < workers_.size(); ++i)
			tasks_.push([] {});
		for (auto& t : workers_)
			t.join();
	}

	void submit(std::function<void()> task) {
		tasks_.push(std::move(task));
	}

	template<typename F, typename... Args>//这个 submit 可以接收 任意函数 / lambda / 函数对象，以及这个函数需要的 任意参数
	auto submit(F&& f, Args&&... args)//std::invoke_result<F, Args...>::type 推导这个函数F最终返回什么类型
		-> std::future<typename std::invoke_result<F, Args...>::type>//submit 返回一个 future，future 里装着这个任务最终的返回值
		//future 是一个 “未来才能拿到结果的盒子”,子线程往里塞结果,主线程用.get() 取
	{
		using RetType = typename std::invoke_result<F, Args...>::type;//给函数的返回值起个名字，方便使用。

		//packaged_task 是“函数 + future 的绑定体”。它能做到：1、包住一个函数。2、自动把返回值放进 future。
		//std::packaged_task<RetType()>：一个 “不带参数、返回 RetType 的任务”。
		//std::make_shared是shared_ptr的一种，shared_ptr保证只要还有线程用它，它就不会被释放。
		auto task = std::make_shared<std::packaged_task<RetType()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)//std::bind(...)：把函数和参数 提前绑死
		);
		//线程通信的桥梁，res.get()在主线程拿到子线程的future的结果。
		std::future<RetType> res = task->get_future();//从 packaged_task 里 拿到 future，get_future()是packaged_task的成员函数。
		tasks_.push([task]() {(*task)();});//生成一个函数，这个函数记住了 task，将来被调用时，就去执行这个 packaged_task。
		return res;
	}
};
