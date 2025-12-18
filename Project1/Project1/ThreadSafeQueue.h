#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class ThreadSafeQueue {
private:
	std::queue<T> queue_;
	mutable std::mutex mtx_;
	std::condition_variable cv_;
public:
	void push(T value) {
		std::lock_guard<std::mutex> lock(mtx_);
		queue_.push(std::move(value));
		cv_.notify_one();

	}

	T pop() {
		std::unique_lock<std::mutex> lock(mtx_);
		cv_.wait(lock, [this] {return !queue_.empty();});
		T val = std::move(queue_.front());
		queue_.pop();
		return val;
	}

	bool empty() const {
		std::lock_guard<std::mutex> lock(mtx_);
		return queue_.empty();
	}
};