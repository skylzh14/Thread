#include <iostream>
#include <vector>
#include <future>
#include "threadPool.h"

int main() {
	threadPool pool(4);
	std::vector<std::future<int>> results;

	for (int i = 0; i <= 10; ++i) {
		results.push_back(pool.submit([i] {
			return i * i;
			}));
	}

	for (auto& res : results) {
		std::cout << "Result:" << res.get() << std::endl;
	}
	return 0;
}