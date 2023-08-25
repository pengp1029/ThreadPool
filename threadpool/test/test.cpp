#include<iostream>
#include"threadpool.h"
#include<chrono>
#include<thread>
using  Ulong = unsigned long long;

class MyTask :public Task {
public:
    MyTask(int begin, int end) :
        begin_(begin), end_(end) {
    }
    // 如何设计run函数的返回值可以表示任意类型
    Any run() {
        std::cout << "tid: " << std::this_thread::get_id() << "begin!" << std::endl;
        //std::cout << "tid: " << std::this_thread::get_id() << "end!" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        Ulong sum = 0;
        for (Ulong i = begin_; i <= end_; i++)
            sum += i;
        std::cout << "tid: " << std::this_thread::get_id() << "end! " << std::endl;
        return sum;
    }
private:
    int begin_;
    int end_;
};
int main() {
    {
        ThreadPool pool;
        // 用户自己可以设置线程池的模式
        pool.setMode(PoolMode::MODE_CACHED);
        // 启动线程池
        pool.start(4);

        // 如何实现result机制
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 10000000));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(10000001, 20000000));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(20000001, 30000000));
        pool.submitTask(std::make_shared<MyTask>(20000001, 30000000));
        pool.submitTask(std::make_shared<MyTask>(20000001, 30000000));
        pool.submitTask(std::make_shared<MyTask>(20000001, 30000000));
        // 随着task执行完，task对象没了，如果result依赖于task，那么result对象也没了
        /*Ulong sum1 = res1.get().cast_<Ulong>();
        Ulong sum2 = res2.get().cast_<Ulong>();
        Ulong sum3 = res3.get().cast_<Ulong>();

        // Master - slave线程模型
        // Master用来分解任务，然后给各个Slave线程分配任务
        // 等待各个Slave线程执行完任务，返回结果
        // Master线程合并各个任务结果，输出
        std::cout << sum1 + sum2 + sum3 << std::endl;

        Ulong sum = 0;
        for (Ulong i = 1; i <= 30000000; i++)
            sum += i;
        std::cout << sum << std::endl;*/
    }



    /*pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());*/
    //std::this_thread::sleep_for(std::chrono::seconds(5));
    //getchar();
    return 0;
}