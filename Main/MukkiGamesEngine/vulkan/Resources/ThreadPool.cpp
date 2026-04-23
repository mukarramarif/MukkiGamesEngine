#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t numThreads)
    : stop(false), activeTasks(0)
{
    // Start the worker threads
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }

    // Wake up all threads so they can exit
    condition.notify_all();

    // Wait for all threads to finish
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::workerLoop()
{
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(queueMutex);

            // Wait until there's a task or the pool is stopped
            condition.wait(lock, [this] { 
                return stop || !tasks.empty(); 
            });

            // If stopped and no tasks left, exit the thread
            if (stop && tasks.empty()) {
                return;
            }

            // Pop the task from the queue
            task = std::move(tasks.front());
            tasks.pop();
        }

        // Execute the task outside the lock
        task();

        // Notify wait condition if tasks are done
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            activeTasks--;
            if (activeTasks == 0 && tasks.empty()) {
                waitCondition.notify_all();
            }
        }
    }
}

void ThreadPool::waitAll()
{
    std::unique_lock<std::mutex> lock(queueMutex);
    waitCondition.wait(lock, [this] {
        return tasks.empty() && activeTasks == 0;
    });
}