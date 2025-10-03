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

#include <algorithm>
#include <functional>
#include <memory>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>

#include <thread>

#include <future>
#include <queue>
#include <unordered_set>
#include <vector>

namespace neko::core::thread {

    using TaskId = neko::uint64;

    struct Task {
        std::function<void()> function;
        neko::Priority priority;
        TaskId id;

        Task() : function(nullptr), priority(neko::Priority::Normal), id(0) {}

        Task(std::function<void()> func, neko::Priority prio, TaskId taskId)
            : function(std::move(func)), priority(prio), id(taskId) {}

        bool operator<(const Task &other) const {
            if (priority != other.priority) {
                return priority < other.priority;
            }
            return id > other.id;
        }
    };

    class WorkerInfo {
    private:
        std::unique_ptr<std::thread> thread;
        neko::uint64 id;
        std::queue<Task> personalTaskQueue;
        std::mutex personalTaskMutex;

    public:
        WorkerInfo(std::unique_ptr<std::thread> &&t, neko::uint64 i)
            : thread(std::move(t)), id(i) {}

        WorkerInfo(WorkerInfo &&other) noexcept
            : thread(std::move(other.thread)),
              id(other.id),
              personalTaskQueue(std::move(other.personalTaskQueue)) {}
        WorkerInfo &operator=(WorkerInfo &&other) noexcept {
            if (this != &other) {
                thread = std::move(other.thread);
                id = other.id;
                personalTaskQueue = std::move(other.personalTaskQueue);
            }
            return *this;
        }

        WorkerInfo(const WorkerInfo &) = delete;
        WorkerInfo &operator=(const WorkerInfo &) = delete;

        void postTask(const Task &task) {
            std::lock_guard<std::mutex> lock(personalTaskMutex);
            personalTaskQueue.push(task);
        }

        void postTask(Task &&task) {
            std::lock_guard<std::mutex> lock(personalTaskMutex);
            personalTaskQueue.push(std::move(task));
        }

        void tryRunPersonalTasks() {
            std::lock_guard<std::mutex> lock(personalTaskMutex);
            while (!personalTaskQueue.empty()) {
                Task task = std::move(personalTaskQueue.front());
                personalTaskQueue.pop();
                if (task.function) {
                    task.function();
                }
            }
        }

        bool isActive() const noexcept {
            return thread && thread->joinable();
        }
        void cleanup(bool waitForCompletion = true) noexcept {
            if (!isActive()) {
                return;
            }
            try {
                if (waitForCompletion) {
                    thread->join();
                } else {
                    thread->detach();
                }
            } catch (const std::system_error &) {
                return;
            }
        }
    };

    class ThreadPool {
    private:
        std::vector<WorkerInfo> workers;
        std::priority_queue<Task> globalTaskQueue;

        mutable std::mutex workerMutex;
        mutable std::mutex globalTaskQueueMutex;
        std::condition_variable globalTaskQueueCondVar;

        std::atomic<bool> isStop{false};
        std::atomic<TaskId> nextTaskId{0};
        std::atomic<neko::uint64> activeTasks{0};
        std::atomic<neko::uint64> maxQueueSize{100000};

        void workerThread() {
            while (true) {
                Task task;

                {
                    std::unique_lock<std::mutex> lock(globalTaskQueueMutex);

                    globalTaskQueueCondVar.wait(lock, [this] {
                        return isStop.load() || !globalTaskQueue.empty();
                    });

                    if (isStop.load() && globalTaskQueue.empty()) {
                        return;
                    }

                    if (!globalTaskQueue.empty()) {
                        task = std::move(const_cast<Task &>(globalTaskQueue.top()));
                        globalTaskQueue.pop();
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
                workers.emplace_back(std::make_unique<std::thread>(&ThreadPool::workerThread, this), i);
            }
        }

        ~ThreadPool() {
            stop();
        }

        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;
        ThreadPool(ThreadPool &&) = delete;
        ThreadPool &operator=(ThreadPool &&) = delete;

        template <typename F, typename... Args>
        auto submit(F &&function, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>> {
            return submitWithPriority(neko::Priority::Normal, std::forward<F>(function), std::forward<Args>(args)...);
        }

        template <typename F, typename... Args>
        auto submitWithPriority(neko::Priority priority, F &&function, Args &&...args)
            -> std::future<std::invoke_result_t<F, Args...>> {

            using ReturnType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

            if (isStop.load()) {
                throw ex::ProgramExit("Cannot submit tasks to stopped thread pool");
            }

            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                [function, args...]() mutable -> ReturnType {
                    return std::invoke(std::forward<F>(function), std::forward<Args>(args)...);
                });

            std::future<ReturnType> result = task->get_future();

            {
                std::unique_lock<std::mutex> lock(globalTaskQueueMutex);

                if (globalTaskQueue.size() >= maxQueueSize.load()) {
                    throw ex::TaskRejected("Task queue is full");
                }

                TaskId taskId = nextTaskId++;
                globalTaskQueue.emplace([task]() { (*task)(); }, priority, taskId);
            }

            globalTaskQueueCondVar.notify_one();
            return result;
        }

        void waitForCompletion() {
            while (true) {
                std::unique_lock<std::mutex> lock(globalTaskQueueMutex);
                if (globalTaskQueue.empty() && activeTasks.load() == 0) {
                    break;
                }
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        void stop(bool waitForTasks = true) {

            isStop.store(true);
            globalTaskQueueCondVar.notify_all();

            for (auto &worker : workers) {
                if (worker.isActive()) {
                    worker.cleanup(waitForTasks);
                }
            }
        }

        neko::uint64 getThreadCount() const noexcept {
            return workers.size();
        }

        neko::uint64 getPendingTaskCount() const {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(globalTaskQueueMutex));
            return globalTaskQueue.size();
        }

        bool isEmpty() const {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(globalTaskQueueMutex));
            return globalTaskQueue.empty() && activeTasks.load() == 0;
        }

        void setMaxQueueSize(neko::uint64 maxSize) noexcept {
            maxQueueSize.store(maxSize);
        }

        neko::uint64 getMaxQueueSize() const noexcept {
            return maxQueueSize.load();
        }

        bool isQueueFull() const {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(globalTaskQueueMutex));
            return globalTaskQueue.size() >= maxQueueSize.load();
        }

        double getThreadUtilization() const noexcept {
            auto totalThreads = workers.size();
            auto activeThreads = activeTasks.load();
            if (totalThreads == 0)
                return 0.0;
            return static_cast<double>(activeThreads) / totalThreads;
        }

        double getQueueUtilization() const {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(globalTaskQueueMutex));
            auto maxSize = maxQueueSize.load();
            if (maxSize == 0)
                return 0.0;
            return static_cast<double>(globalTaskQueue.size()) / maxSize;
        }
    };

} // namespace neko::core::thread
