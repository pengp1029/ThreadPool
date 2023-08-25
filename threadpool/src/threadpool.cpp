#include"threadpool.h"
#include<thread>
#include<iostream>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_OVER_TIME = 5; //s
ThreadPool::ThreadPool()
    : initThreadSize_(0),
    taskSize_(0),
    taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
    poolMode_(PoolMode::MODE_FIXED),
    isRunning_(false),
    curThreadSize_(0),
    availThreadSize_(0),
    threadSizeThreshHold_(THREAD_MAX_THRESHHOLD){}

ThreadPool::~ThreadPool() {
    isRunning_ = false;
     /*
     * 等待线程池所有的线程返回
     * 有两种状态：阻塞&正在执行任务中
     */
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    notEmpty_.notify_all();
    exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

void ThreadPool::start(size_t initThreadSize) {
    isRunning_ = true;
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;

    // 创建线程对象
    for (int i = 0; i < initThreadSize_; i++) {
        // 创建thread线程对象的时候，把线程函数给到thread线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
        //threads_.emplace_back(std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this)));
        
    }

    // 启动所有线程
    for (int i = 0; i < initThreadSize_; i++) {
        threads_[i]->start();
        availThreadSize_++; // 初始空闲线程的数量
    }
}

void ThreadPool::setMode(PoolMode mode) {
    if (checkRunningState())
        return;
    poolMode_ = mode;
}

void ThreadPool::setTaskQueThreadSize(size_t threshhold) {
    taskQueMaxThreshHold_ = threshhold;
}

void ThreadPool::setThreadSizeThreshHold(size_t threshhold) {
    if (checkRunningState())
        return;
    if (poolMode_ == PoolMode::MODE_CACHED)
        return;
    threadSizeThreshHold_ = threshhold;
}

bool ThreadPool::checkRunningState()const {
    return isRunning_;
}

// 传入任务 生产者
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {
    // 获取锁
    std::unique_lock<std::mutex > lock(taskQueMtx_);
    
    // 线程通信 等待任务队列有位置
    /*while (taskQue_.size() == taskQueMaxThreshHold_) {
        notFull_.wait(lock); // 等待状态 既要锁又要条件变量满足
    }*/

    // 用户提交最长不能阻塞超过1s，否则判断提交任务失败
    // wait 锁：加循环判断条件；wait_for 等条件满足，最多等一段时间
    if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue_.size() < (size_t) taskQueMaxThreshHold_; })) {
        // 提交任务失败
        std::cerr << "task queue is full, submit task fail." << std::endl;
        return Result(sp,false);
    }
    
    // 如果有空余，把任务放入任务队列中
    taskQue_.emplace(sp);
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
    return Result(sp);
}

// 线程中的线程函数，从任务队列消耗任务 消费者
void ThreadPool::threadFunc(int threadid) {
    /*std::cout << "begin threadFunc tid: " << std::this_thread::get_id() << std::endl;
    std::cout << "end threadFunc tid: " << std::this_thread::get_id() << std::endl;*/
    auto lastTime = std::chrono::high_resolution_clock().now();
    
    //while(isRunning_) {
    for(;;){
        std::shared_ptr<Task> task;
        {
            // 获取锁
            std::unique_lock<std::mutex> lock(taskQueMtx_);

            std::cout << "tid: " << std::this_thread::get_id() << "try catch task.." << std::endl;
            
            // cache模式下，如果等待60s还没有任务，则把多余的线程结束回收掉
            // 当前时间 - 上一次线程执行的时间 > t s 对超过initThreadSize_数量的线程要进行回收
            // 每1s返回一次
            //while (isRunning_&&taskQue_.size() == 0) {
            while (taskQue_.size() == 0) {
                if (!isRunning_) {
                    threads_.erase(threadid);
                    std::cout << "threadid: " << std::this_thread::get_id() << "exit!" << std::endl;
                    exitCond_.notify_all();
                    return;
                }
                if (poolMode_ == PoolMode::MODE_CACHED) {
                    // 超时返回
                    if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if (dur.count() >= THREAD_OVER_TIME
                            && curThreadSize_ > initThreadSize_) {
                            /*
                            * 回收当前线程
                            * 记录线程数量的变量值修改
                            * 线程对象从线程列表容器中删除
                            * 找到threadFunc对应的thread对象，然后删除
                            */
                            threads_.erase(threadid);
                            curThreadSize_--;
                            availThreadSize_--;

                            std::cout << "threadid: " << std::this_thread::get_id() << "exit!" << std::endl;
                            return;
                        }
                    }
                }
                else {
                    // 等待notEmpty
                    notEmpty_.wait(lock);
                }
                // 线程池结束 回收资源
                /*if (!isRunning_) {
                    threads_.erase(threadid);
                    std::cout << "threadid: " << std::this_thread::get_id() << "exit!" << std::endl;
                    return;
                }*/
            }
            /*if (!isRunning_) {
                break;
            }*/
            
            
            availThreadSize_--;
            
            std::cout << "tid: " << std::this_thread::get_id() << "catch task success!" << std::endl;
            
            // 任务队列中取任务
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;

            // 如果有剩余任务，通知其他线程执行任务
            if (taskQue_.size() > 0) {
                notEmpty_.notify_all();
            }

            notFull_.notify_all();
        }
        // 当前线程负责该任务
        if (task != nullptr) {
            //task->run(); // 执行任务，把任务的返回值setVal给到Result
            task->exec();
        }
        availThreadSize_++;
        lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
    }

    //threads_.erase(threadid);
    //std::cout << "threadid: " << std::this_thread::get_id() << "exit!" << std::endl;
}


// =========================== 线程方法实现 ===========================
size_t Thread::generateId_ = 0;

void Thread::start() {
    // 创建一个线程来执行一个线程函数
    std::thread t(func_,threadId_); // c++11来说，线程对象t和线程函数func_
    t.detach(); // 设置分离线程
}

Thread::Thread(ThreadFunc func) :
    func_(func),
    threadId_(generateId_++){}

size_t Thread::getId()const {
    return threadId_;
}

Thread::~Thread() {}


// ============================ result 方法实现 ==================
Result::Result(std::shared_ptr<Task> task, bool isValid) 
    :isValid_(isValid),task_(task){
    task_->setResult(this);
}

Any Result::get() {
    if (!isValid_) {
        return "";
    }
    sem_.wait(); // 如果task没有执行完，这里会阻塞用户的线程
    return std::move(any_);
}

void Result::setVal(Any any) {
    // 存储task的返回值
    this->any_ = std::move(any);
    sem_.post(); // 已经获取了任务的返回值，增加信号量资源
}


// ============================ Task 方法实现 ==================
void Task::exec() {
    if (result_ != nullptr) {
        result_->setVal(run());
    }
}

void Task::setResult(Result* res) {
    result_ = res;
}

Task::Task():result_(nullptr){}