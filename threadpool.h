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

// �̳߳�֧�ֵ�ģʽ
enum class PoolMode {
    MODE_FIXED, // �̶�����
    MODE_CACHED // ��̬����
};

//�߳�����
class Thread {
public:
    // �̺߳�����������
    using ThreadFunc = std::function<void(int)>;

    Thread(ThreadFunc);
    ~Thread();

    void start(); // �����߳�
    size_t getId()const;
private:
    ThreadFunc func_;
    static size_t generateId_;
    size_t threadId_; // �����߳�id
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

// �̳߳�����
class ThreadPool {
public:
    ThreadPool(); // �̳߳ع���
    ~ThreadPool(); // �̳߳�����

    void start(size_t initThreadSize = std::thread::hardware_concurrency()); // �����̳߳�
    void setMode(PoolMode mode); // �����̳߳�ģʽ
    void setTaskQueThreadSize(size_t threshhold); // �����������������ֵ
    //Result submitTask(std::shared_ptr<Task> sp); // �ύ����
    template<typename Func,typename... Args>
    auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))> {
        // �������,���������������
        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType()>>(std::bind(std::forward<Func>(func),std::forward<Args>(args)...));
        std::future<RType> result = task->get_future();

        // ��ȡ��
        std::unique_lock<std::mutex > lock(taskQueMtx_);

        // �û��ύ�������������1s�������ж��ύ����ʧ��
        // wait ������ѭ���ж�������wait_for ���������㣬����һ��ʱ��
        if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshHold_; })) {
            // �ύ����ʧ��
            std::cerr << "task queue is full, submit task fail." << std::endl;
            auto task = std::make_shared<std::packaged_task<RType()>>([]()->RType {return RType(); });
            (*task)();
            return task->get_future();
        }

        // ����п��࣬������������������
        //taskQue_.emplace(sp);
        taskQue_.emplace([task]() {
            (*task)();
            });
        taskSize_++;
        // �·�������������в��գ�notEmpty_֪ͨ
        notEmpty_.notify_all();

        // cacheģʽ��Ҫ�������������Ϳ����߳��������ж��Ƿ���Ҫ�����µ��߳�
        if (poolMode_ == PoolMode::MODE_CACHED
            && taskSize_ > availThreadSize_
            && curThreadSize_ < threadSizeThreshHold_) {

            std::cout << ">>>  create new thread...." << std::endl;
            // �������߳�
            auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
            int threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr));
            threads_[threadId]->start();
            // �޸��̸߳�����صı���
            curThreadSize_++;
            availThreadSize_++;
        }
        return result;
    }

    void setThreadSizeThreshHold(size_t threshhold); // �����߳���������

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    std::unordered_map<int, std::unique_ptr<Thread>>threads_;
    //std::vector<std::unique_ptr<Thread>> threads_; // �߳��б�
    size_t initThreadSize_; // ��ʼ�߳�����
    size_t threadSizeThreshHold_; // �߳�����������ֵ

    using Task = std::function<void()>;
    std::queue<Task> taskQue_ ;// �Լ���װ�Լ�������������
    std::atomic_uint taskSize_; // ���������
    size_t taskQueMaxThreshHold_; // ��������������ֵ

    std::mutex taskQueMtx_; // ��֤������е��̰߳�ȫ
    std::condition_variable notFull_; // ������в���
    std::condition_variable notEmpty_; // ������в���
    std::condition_variable exitCond_; // �ȴ��߳���Դȫ������

    PoolMode poolMode_;
    std::atomic_bool isRunning_;  // ��ʾ��ǰ�̳߳ص�����״̬
    std::atomic_uint availThreadSize_; // ���е��߳�����
    std::atomic_uint curThreadSize_; // ��ǰ�߳�����

    void threadFunc(int threadid);

    // ���pool������״̬
    bool checkRunningState()const;

    

};

#endif