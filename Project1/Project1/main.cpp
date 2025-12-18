//#include <iostream>
//#include <thread>
//#include <mutex>
//
//std::mutex mtx;
//
//void print_num(int x) {
//	std::lock_guard<std::mutex> lock(mtx);
//	std::cout << "Thread" << std::this_thread::get_id() << "prints" << x << std::endl;
//}
//
//int main() {
//	std::thread t1(print_num, 1);
//	std::thread t2(print_num, 2);
//
//	t1.join();
//	t2.join();
//
//	return 0;
//}

//测试ThreadSafeQueue
//#include "ThreadSafeQueue.h"
//#include <thread>
//#include <iostream>
//#include <algorithm>
//#include <vector>
//#include <numeric>
//
//int main() {
//	ThreadSafeQueue<int> q;
//	std::vector<int> result;
//	std::mutex resultMtx;
//
//	//两个生产者
//	auto prod = [&](int start) {
//		for (int i = 0; i < 50; ++i) q.push(start + i);
//		};
//	std::thread p1(prod, 0);
//	std::thread p2(prod, 50);
//
//	//一个消费者
//	std::thread c([&] {
//		for (int i = 0; i < 100; ++i) {
//			int val = q.pop();
//			std::lock_guard<std::mutex> lk(resultMtx);
//			result.push_back(val);
//		}
//		});
//
//	p1.join();
//	p2.join();
//	c.join();
//
//	//排序后验证0..99 全部出现且仅出现一次
//	std::sort(result.begin(), result.end());
//	bool ok = (result.size() == 100);
//	for (int i = 0; i < 100 && ok; ++i)
//		ok &= (result[i] == i);
//	std::cout << (ok ? "PASS" : "FAIL") << std::endl;
//}

//测试ThreadPool
//#include "ThreadPool.h"
//#include <iostream>
//int main() {
//	ThreadPool pool(4);                 // 4 条工作线程
//	pool.submit([] { std::cout << "hello\n"; });
//	// 程序结束时会自动 join，所有任务执行完才退出
//
//	// 等 100 ms，给工作线程时间取票+执行
//	std::this_thread::sleep_for(std::chrono::milliseconds(100));
//	return 0;
//}

//#include "ThreadPool.h"
//#include <iostream>

//int main() {
//    ThreadPool pool(4);
//
//    for (int i = 0; i < 10; ++i) {
//        pool.submit([i] {
//            std::cout << "Task " << i << " running in thread "
//                << std::this_thread::get_id() << std::endl;
//            });
//    }
//
//    std::this_thread::sleep_for(std::chrono::seconds(1));
//    return 0;
//}
//int main() {
//    ThreadPool pool(4);
//
//    auto f1 = pool.submit([](int x) { return x * x; }, 5);
//    auto f2 = pool.submit([](int x) { return x * x; }, 10);
//
//    std::cout << "Result f1: " << f1.get() << std::endl;
//    std::cout << "Result f2: " << f2.get() << std::endl;
//
//    return 0;
//}

#include "ThreadPool.h"
#include <iostream>
#include <vector>

int main() {
	ThreadPool pool(4);
	std::vector<std::future<int>> results;
	for (int i = 0; i <= 10; ++i) {
		results.push_back(pool.submit([i] {
			std::cout << "Task" << i << "running in thread"
				<< std::this_thread::get_id() << std::endl;
			return i * i;
			}));
	}

	for (auto& res : results) {
		std::lock_guard<std::mutex> lk(std::mutex);
		std::cout << "Result: " << res.get() << std::endl;
	}
	return 0;
}



