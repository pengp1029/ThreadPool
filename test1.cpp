#include<iostream>
#include"threadpool.h"
#include<chrono>
#include<thread>
#include<future>

int sum1(int a, int b) {
	return a + b;
}
int sum2(int a, int b, int c) {
	return a + b + c;
}
int main() {
	ThreadPool pool;
	pool.start(4);
	std::future<int> r  = pool.submitTask(sum1, 1, 2);
	std::cout << r.get() << std::endl;
	//std::packaged_task<int(int, int)> task(sum1);
	//std::future<int> res = task.get_future();
	////task(10, 20);
	//std::thread t(std::move(task), 10, 20);
	//t.detach();
	//std::cout << res.get() << std::endl;

	/*std::thread t1(sum1, 10, 20);
	std::thread t2(sum2, 1, 2, 3);
	t1.join();
	t2.join();*/

}