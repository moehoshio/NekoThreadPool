/**
 * @brief Neko Thread pool
 * @file threadPool.hpp
 * @author moehoshio
 * @copyright Copyright (c) 2025 Hoshi
 * @license MIT OR Apache-2.0
 */
#pragma once

#include <neko/schema/exception.hpp>
#include <neko/schema/types.hpp>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace neko::core::thread {

    using TaskId = neko::uint64;

    struct Task {
        std::function<void()> function;
        neko::Priority priority;
        TaskId id;

        Task(std::function<void()> func, neko::Priority prio, TaskId taskId)
            : function(std::move(func)), priority(prio), id(taskId) {}

        bool operator<(const Task &other) const {
            if (priority != other.priority) {
                return priority < other.priority;
            }
            return id > other.id;
        }
    };

    class ThreadPool {
    private:
        std::vector<std::thread> workers;
        std::priority_queue<Task> tasks;
        
        std::mutex queueMutex;
        std::condition_variable queueCondVar;
        
        std::atomic<bool> isStop{false};
        std::atomic<TaskId> nextTaskId{0};
        std::atomic<neko::uint64> activeTasks{0};
        std::atomic<neko::uint64> maxQueueSize{100000};

        void workerThread() {
            while (true) {
                Task task{nullptr, neko::Priority::Normal, 0};
                
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    
                    queueCondVar.wait(lock, [this] {
                        return isStop.load() || !tasks.empty();
                    });
                    
                    if (isStop.load() && tasks.empty()) {
                        return;
                    }
                    
                    if (!tasks.empty()) {
                        task = std::move(const_cast<Task&>(tasks.top()));
                        tasks.pop();
                        ++activeTasks;
                    }
                }
                
                if (task.function) {
                    task.function();
                    --activeTasks;
                }
            }
        }

    public:
        explicit ThreadPool(neko::uint64 threadCount = std::thread::hardware_concurrency()) {
            if (threadCount == 0) {
                threadCount = 1;
            }
            
            workers.reserve(threadCount);
            for (neko::uint64 i = 0; i < threadCount; ++i) {
                workers.emplace_back(&ThreadPool::workerThread, this);
            }
        }

        ~ThreadPool() {
            stop();
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        template <typename F, typename... Args>
        auto submit(F&& function, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
            return submitWithPriority(neko::Priority::Normal, std::forward<F>(function), std::forward<Args>(args)...);
        }

        template <typename F, typename... Args>
        auto submitWithPriority(neko::Priority priority, F&& function, Args&&... args) 
            -> std::future<std::invoke_result_t<F, Args...>> {
            
            using ReturnType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

            if (isStop.load()) {
                throw ex::ProgramExit("Cannot submit tasks to stopped thread pool");
            }

            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                [func = std::forward<F>(function), ... args = std::forward<Args>(args)]() mutable -> ReturnType {
                    return std::invoke(std::forward<F>(func), std::forward<Args>(args)...);
                }
            );

            std::future<ReturnType> result = task->get_future();

            {
                std::unique_lock<std::mutex> lock(queueMutex);
                
                if (tasks.size() >= maxQueueSize.load()) {
                    throw ex::TaskRejected("Task queue is full");
                }
                
                TaskId taskId = nextTaskId++;
                tasks.emplace([task]() { (*task)(); }, priority, taskId);
            }
            
            queueCondVar.notify_one();
            return result;
        }

        void waitForCompletion() {
            while (true) {
                std::unique_lock<std::mutex> lock(queueMutex);
                if (tasks.empty() && activeTasks.load() == 0) {
                    break;
                }
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        void stop(bool waitForTasks = true) {
            if (isStop.exchange(true)) {
                return;
            }

            if (waitForTasks) {
                waitForCompletion();
            }

            queueCondVar.notify_all();

            for (auto& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }

        neko::uint64 getThreadCount() const noexcept {
            return workers.size();
        }

        neko::uint64 getPendingTaskCount() const {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queueMutex));
            return tasks.size();
        }

        bool isEmpty() const {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queueMutex));
            return tasks.empty() && activeTasks.load() == 0;
        }

        void setMaxQueueSize(neko::uint64 maxSize) noexcept {
            maxQueueSize.store(maxSize);
        }

        neko::uint64 getMaxQueueSize() const noexcept {
            return maxQueueSize.load();
        }

        bool isQueueFull() const {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queueMutex));
            return tasks.size() >= maxQueueSize.load();
        }

        double getThreadUtilization() const noexcept {
            auto totalThreads = workers.size();
            auto activeThreads = activeTasks.load();
            if (totalThreads == 0) return 0.0;
            return static_cast<double>(activeThreads) / totalThreads;
        }

        double getQueueUtilization() const {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queueMutex));
            auto maxSize = maxQueueSize.load();
            if (maxSize == 0) return 0.0;
            return static_cast<double>(tasks.size()) / maxSize;
        }
    };

} // namespace neko::core::thread
