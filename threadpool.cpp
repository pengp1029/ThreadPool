#include"threadpool.h"
#include<thread>
#include<iostream>

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
     * �ȴ��̳߳����е��̷߳���
     * ������״̬������&����ִ��������
     */
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    notEmpty_.notify_all();
    exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

void ThreadPool::start(size_t initThreadSize) {
    isRunning_ = true;
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;

    // �����̶߳���
    for (int i = 0; i < initThreadSize_; i++) {
        // ����thread�̶߳����ʱ�򣬰��̺߳�������thread�̶߳���
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
        //threads_.emplace_back(std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this)));
        
    }

    // ���������߳�
    for (int i = 0; i < initThreadSize_; i++) {
        threads_[i]->start();
        availThreadSize_++; // ��ʼ�����̵߳�����
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

// �߳��е��̺߳���������������������� ������
void ThreadPool::threadFunc(int threadid) {
    /*std::cout << "begin threadFunc tid: " << std::this_thread::get_id() << std::endl;
    std::cout << "end threadFunc tid: " << std::this_thread::get_id() << std::endl;*/
    auto lastTime = std::chrono::high_resolution_clock().now();
    
    //while(isRunning_) {
    for(;;){
        Task task;
        {
            // ��ȡ��
            std::unique_lock<std::mutex> lock(taskQueMtx_);

            std::cout << "tid: " << std::this_thread::get_id() << "try catch task.." << std::endl;
            
            // cacheģʽ�£�����ȴ�60s��û��������Ѷ�����߳̽������յ�
            // ��ǰʱ�� - ��һ���߳�ִ�е�ʱ�� > t s �Գ���initThreadSize_�������߳�Ҫ���л���
            // ÿ1s����һ��
            //while (isRunning_&&taskQue_.size() == 0) {
            while (taskQue_.size() == 0) {
                if (!isRunning_) {
                    threads_.erase(threadid);
                    std::cout << "threadid: " << std::this_thread::get_id() << "exit!" << std::endl;
                    exitCond_.notify_all();
                    return;
                }
                if (poolMode_ == PoolMode::MODE_CACHED) {
                    // ��ʱ����
                    if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if (dur.count() >= THREAD_OVER_TIME
                            && curThreadSize_ > initThreadSize_) {
                            /*
                            * ���յ�ǰ�߳�
                            * ��¼�߳������ı���ֵ�޸�
                            * �̶߳�����߳��б�������ɾ��
                            * �ҵ�threadFunc��Ӧ��thread����Ȼ��ɾ��
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
                    // �ȴ�notEmpty
                    notEmpty_.wait(lock);
                }
                // �̳߳ؽ��� ������Դ
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
            
            // ���������ȡ����
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;

            // �����ʣ������֪ͨ�����߳�ִ������
            if (taskQue_.size() > 0) {
                notEmpty_.notify_all();
            }

            notFull_.notify_all();
        }
        // ��ǰ�̸߳��������
        if (task != nullptr) {
            //task->run(); // ִ�����񣬰�����ķ���ֵsetVal����Result
            task();
        }
        availThreadSize_++;
        lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��
    }

    //threads_.erase(threadid);
    //std::cout << "threadid: " << std::this_thread::get_id() << "exit!" << std::endl;
}


// =========================== �̷߳���ʵ�� ===========================
size_t Thread::generateId_ = 0;

void Thread::start() {
    // ����һ���߳���ִ��һ���̺߳���
    std::thread t(func_,threadId_); // c++11��˵���̶߳���t���̺߳���func_
    t.detach(); // ���÷����߳�
}

Thread::Thread(ThreadFunc func) :
    func_(func),
    threadId_(generateId_++){}

size_t Thread::getId()const {
    return threadId_;
}

Thread::~Thread() {}
