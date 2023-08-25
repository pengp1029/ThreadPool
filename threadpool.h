#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<unordered_map>
#include<thread>
#include<future>
#include<iostream>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_OVER_TIME = 5; //s

// 线程池支持的模式
enum class PoolMode {
    MODE_FIXED, // 固定数量
    MODE_CACHED // 动态增长
};

//线程类型
class Thread {
public:
    // 线程函数对象类型
    using ThreadFunc = std::function<void(int)>;

    Thread(ThreadFunc);
    ~Thread();

    void start(); // 启动线程
    size_t getId()const;
private:
    ThreadFunc func_;
    static size_t generateId_;
    size_t threadId_; // 保存线程id
};

/*
example:
ThreadPool pool;
pool.start(4);
class MyTask:public Task{
    public:
        void run(){//}
}
pool.submitTask(std::make_shared<MyTask>());
*/

// 线程池类型
class ThreadPool {
public:
    ThreadPool(); // 线程池构造
    ~ThreadPool(); // 线程池析构

    void start(size_t initThreadSize = std::thread::hardware_concurrency()); // 开启线程池
    void setMode(PoolMode mode); // 设置线程池模式
    void setTaskQueThreadSize(size_t threshhold); // 设置任务队列上限阈值
    //Result submitTask(std::shared_ptr<Task> sp); // 提交任务
    template<typename Func,typename... Args>
    auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))> {
        // 打包任务,放入任务队列里面
        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType()>>(std::bind(std::forward<Func>(func),std::forward<Args>(args)...));
        std::future<RType> result = task->get_future();

        // 获取锁
        std::unique_lock<std::mutex > lock(taskQueMtx_);

        // 用户提交最长不能阻塞超过1s，否则判断提交任务失败
        // wait 锁：加循环判断条件；wait_for 等条件满足，最多等一段时间
        if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshHold_; })) {
            // 提交任务失败
            std::cerr << "task queue is full, submit task fail." << std::endl;
            auto task = std::make_shared<std::packaged_task<RType()>>([]()->RType {return RType(); });
            (*task)();
            return task->get_future();
        }

        // 如果有空余，把任务放入任务队列中
        //taskQue_.emplace(sp);
        taskQue_.emplace([task]() {
            (*task)();
            });
        taskSize_++;
        // 新放了任务，任务队列不空，notEmpty_通知
        notEmpty_.notify_all();

        // cache模式需要根据任务数量和空闲线程数量，判断是否需要创建新的线程
        if (poolMode_ == PoolMode::MODE_CACHED
            && taskSize_ > availThreadSize_
            && curThreadSize_ < threadSizeThreshHold_) {

            std::cout << ">>>  create new thread...." << std::endl;
            // 创建新线程
            auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
            int threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr));
            threads_[threadId]->start();
            // 修改线程个数相关的变量
            curThreadSize_++;
            availThreadSize_++;
        }
        return result;
    }

    void setThreadSizeThreshHold(size_t threshhold); // 设置线程数量上限

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    std::unordered_map<int, std::unique_ptr<Thread>>threads_;
    //std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
    size_t initThreadSize_; // 初始线程数量
    size_t threadSizeThreshHold_; // 线程数量上限阈值

    using Task = std::function<void()>;
    std::queue<Task> taskQue_ ;// 自己封装自己决定生命周期
    std::atomic_uint taskSize_; // 任务的数量
    size_t taskQueMaxThreshHold_; // 任务数量上限阈值

    std::mutex taskQueMtx_; // 保证任务队列的线程安全
    std::condition_variable notFull_; // 任务队列不满
    std::condition_variable notEmpty_; // 任务队列不空
    std::condition_variable exitCond_; // 等待线程资源全部回收

    PoolMode poolMode_;
    std::atomic_bool isRunning_;  // 表示当前线程池的启动状态
    std::atomic_uint availThreadSize_; // 空闲的线程数量
    std::atomic_uint curThreadSize_; // 当前线程数量

    void threadFunc(int threadid);

    // 检查pool的运行状态
    bool checkRunningState()const;

    

};

#endif