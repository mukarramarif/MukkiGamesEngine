#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <type_traits>

class ThreadPool {
public:
    // Initialize the thread pool with a specific number of threads (defaults to hardware concurrency)
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    // Prevent copying and assignment
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Enqueue a task to be processed by the thread pool.
    // Returns a std::future to retrieve the result or wait for completion.
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

    // Wait until all tasks in the queue are finished
    void waitAll();

private:
    // Worker thread loop
    void workerLoop();

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;
    std::condition_variable waitCondition;

    bool stop;
    size_t activeTasks;
};

// Template implementation must be in the header
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
    using return_type = std::invoke_result_t<F, Args...>;

    // Wrap the task in a shared pointer to std::packaged_task
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        // Add task to the queue
        tasks.emplace([task]() { (*task)(); });
        activeTasks++;
    }

    // Wake up one waiting worker thread
    condition.notify_one();
    return res;
}