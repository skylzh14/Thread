#pragma once
#include <vector>
#include "threadSafeQueue.h"
#include <atomic>
#include <thread>
#include <functional>
#include <future>


class threadPool {
private:
	std::vector<std::thread> workers_;
	threadSafeQueue<std::function<void()>> tasks_;
	std::atomic<bool> stop_{ false };
	
	void worker() {
		while (!stop_) {
			auto task = tasks_.pop();
			task();
		}
	}
public:
	threadPool(int num_threads) {
		for (int i = 0; i < num_threads; ++i) {
			workers_.emplace_back(&threadPool::worker, this);
		}
	}

	~threadPool() {
		stop_ = true;

		for (size_t i = 0; i < workers_.size(); ++i) {
			tasks_.push([] {});
		}
		for (auto& t : workers_)
			t.join();
	}

	template<typename F, typename... Args>
	auto submit(F&& f, Args&&... args)
		-> std::future<typename std::invoke_result<F, Args...>::type>
	{
		using RetType = typename std::invoke_result<F, Args...>::type;

		auto task = std::make_shared<std::packaged_task<RetType()>>(
			std::bind(std::forward<F>(f),std::forward<Args>(args)...)
		);

		std::future<RetType> res = task->get_future();
		tasks_.push([task]() {(*task)();});
		return res;
	}

}; 


