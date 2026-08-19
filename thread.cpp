#include<iostream>
#include<thread>
#include<mutex>
#include<string>
#include<condition_variable>
#include<queue>
#include<vector>
#include<functional>
#include <chrono>
#include <utility>   // std::forward / std::move
class ThreadPool {
    public:
    ThreadPool(int numThreads) : stop(false) {
        for (int i = 0;i < numThreads;i++){
            threads.emplace_back([this]{
                while(1){
                    std::unique_lock<std::mutex> lock(mtx);//lamdba表达式捕获this指针，访问类成员变量
                    condition.wait(lock,[this]{
                        return !tasks.empty() || stop;//PUSHBACK会进行拷贝构造，会节省资源
                    });
                    if(stop && tasks.empty()){
                        return;
                    }
                    std::function<void()>task(std::move(tasks.front()));//std::move()将左值转换为右值引用，避免拷贝构造
                    tasks.pop();//弹出队列头部元素
                    lock.unlock();
                    task();
                }
            });
        }
    }
    ~ThreadPool(){
        {
        std::unique_lock<std::mutex> lock(mtx);
        stop = true;
    }
    condition.notify_all();//唤醒所有线程
    for(auto& t : threads){
        t.join();
    }
}
template<class F,class ...Args>//可变参数模板
void enqueue(F &&f,Args &&...args){
    std::function<void()> task = 
    std::bind(std::forward<F>(f), std::forward<Args>(args)...);//std::bind()将函数和参数绑定为一个可调用对象，std::forward()完美转发参数

    {
        std::unique_lock<std::mutex> lock(mtx);
    tasks.emplace(std::move(task));
    }
    
    condition.notify_one();
}
    private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;//互斥锁
    std::condition_variable condition;//条件变量
    bool stop;
};

std::mutex print_mtx;

int main(){
ThreadPool pool(4);

for(int i = 0; i < 10; i++){
    pool.enqueue([i]{
        {
            std::lock_guard<std::mutex> g(print_mtx);
            std::cout << "task :" << i << " is running" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        {
            std::lock_guard<std::mutex> g(print_mtx);
            std::cout << "task :" << i << " is stop" << std::endl;
        }
    });
} 
}