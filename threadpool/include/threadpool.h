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

// Any类型：可以接受任意数据的类型
class Any {
public:
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;

    // 可以让Any接受任意其他的数据
    template<typename T>
    Any(T data) :base_(std::make_unique<Derive<T>>(data)){}

    template<typename T>
    T cast_() {
        // 如何从Base里面找到所指向的Derive对象，从他里面取出成员变量的类型
        Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
        if (pd == nullptr) {
            throw "type is unmatch!";
        }
        return pd->getData();
    }
private:
    // 基类类型
    class Base {
    public:
        virtual ~Base() = default;
    };
    // 派生类类型
    template<typename T>
    class Derive : public Base {
    public:
        Derive(T data) :data_(data) {}
        T getData() { return data_; }
    private:
        T data_; //  保存了任意的其他类型
    };

    std::unique_ptr<Base> base_;
};

// 实现一个信号量类
class Semaphore {
public:
    Semaphore(int limit = 0) :resLimit_(limit),isExit_(false) {}
    ~Semaphore() {
        isExit_ = true;
    }

    // 获取一个信号量资源
    void wait() {
        if (isExit_)
            return;
        std::unique_lock<std::mutex> lock(mtx_);
        // 等待信号量有资源，没有资源的话会阻塞当前线程
        cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
        resLimit_--;
    }

    // 增加一个信号量资源
    void post() {
        if (isExit_)
            return;
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_++;
        cond_.notify_all();
    }
private:
    std::atomic_bool isExit_;
    int resLimit_;
    std::mutex mtx_;
    std::condition_variable cond_;
};

// 任务抽象基类
// 用户可以自定义任意任务类型，从Task继承而来，重写run方法
extern class Result;
class Task {
public:
    virtual Any run() = 0;
    void setResult(Result* res);
    void exec();
    Task();
    ~Task() = default;
private:
    Result* result_; // Result对象的生命周期>Task
};

// 实现接受任务提交到线程池的task任务执行完成后的返回值类型Result
class Result {
public:
    Result(std::shared_ptr<Task> task, bool isValid = true);
    ~Result() = default;

    // setVal 获取任务执行完的返回值
    void setVal(Any any);

    // get方法，用户调用该方法获取task的返回值
    Any get();

private:
    Any any_; // 存储人物的返回值
    Semaphore sem_; // 线程通信信号量
    std::shared_ptr<Task> task_; // 指向对应的获取返回值的任务对象
    std::atomic_bool isValid_; // 返回值是否有效
};

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
    Result submitTask(std::shared_ptr<Task> sp); // 提交任务
    void setThreadSizeThreshHold(size_t threshhold); // 设置线程数量上限

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    std::unordered_map<int, std::unique_ptr<Thread>>threads_;
    //std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
    size_t initThreadSize_; // 初始线程数量
    size_t threadSizeThreshHold_; // 线程数量上限阈值


    std::queue<std::shared_ptr<Task>> taskQue_; // 裸指针不行，因为需要保持对象的生命周期
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