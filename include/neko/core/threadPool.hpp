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

/**
 * @brief Thread pool
 * @namespace neko::core::thread
 */
namespace neko::core::thread {

    using TaskId = neko::uint64;

    struct Task {
        std::function<void()> function;
        neko::Priority priority;
        TaskId id;

        Task() : function(nullptr), priority(neko::Priority::Normal), id(0) {}

        Task(std::function<void()> func, neko::Priority prio, TaskId taskId)
            : function(std::move(func)), priority(prio), id(taskId) {}

        /**
         * @brief Compare two tasks for priority.
         * @note  priority > id
         */
        bool operator<(const Task &other) const {
            if (priority != other.priority) {
                return priority < other.priority;
            }
            return id > other.id;
        }

        bool hasTask() const noexcept {
            return function != nullptr;
        }
    };

    class WorkerInfo {
    private:
        std::atomic<bool> shouldStop{false};
        std::unique_ptr<std::thread> thread;
        neko::uint64 id;
        std::queue<Task> personalTaskQueue;
        mutable std::shared_mutex personalTaskMutex;

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
            std::lock_guard<std::shared_mutex> lock(personalTaskMutex);
            personalTaskQueue.push(task);
        }

        void postTask(Task &&task) {
            std::lock_guard<std::shared_mutex> lock(personalTaskMutex);
            personalTaskQueue.push(std::move(task));
        }

        void tryRunPersonalTasks() {
            std::lock_guard<std::shared_mutex> lock(personalTaskMutex);
            while (!personalTaskQueue.empty()) {
                Task task = std::move(personalTaskQueue.front());
                personalTaskQueue.pop();
                if (task.hasTask()) {
                    try {
                        task.function();
                    } catch (...) {
                    }
                }
            }
        }

        neko::uint64 getId() const noexcept {
            return id;
        }

        bool isStopping() const noexcept {
            return shouldStop.load();
        }

        bool hasPersonalTasks() const noexcept {
            std::shared_lock<std::shared_mutex> lock(personalTaskMutex);
            return !personalTaskQueue.empty();
        }

        bool isActive() const noexcept {
            return thread && thread->joinable();
        }

        void setShouldStop(bool value) noexcept {
            shouldStop.store(value);
        }

        void cleanup(bool waitForCompletion = true) noexcept {
            setShouldStop(true);

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
        // Worker and global task queue
        std::vector<WorkerInfo> workers;
        std::priority_queue<Task> globalTaskQueue;

        mutable std::shared_mutex workerMutex;
        mutable std::shared_mutex globalTaskQueueMutex;
        std::condition_variable_any globalTaskQueueCondVar;

        // Stop flag and task counters
        std::atomic<bool> isStop{false};
        std::atomic<TaskId> nextTaskId{0};
        std::atomic<neko::uint64> nextWorkerId{0};
        std::atomic<neko::uint64> activeTasks{0};
        std::atomic<neko::uint64> maxQueueSize{100000};

        /**
         * @brief Worker thread function that processes tasks.
         */
        void workerThread(neko::uint64 workerId) {
            WorkerInfo *self = nullptr;

            {
                std::shared_lock<std::shared_mutex> lock(workerMutex);
                for (auto &worker : workers) {
                    if (worker.isActive() && workerId == worker.getId()) {
                        self = &worker;
                        break;
                    }
                }
            }

            while (true) {

                if (self) {
                    self->tryRunPersonalTasks();

                    if (self->isStopping()) {
                        return;
                    }
                }

                Task task;

                {
                    std::unique_lock<std::shared_mutex> lock(globalTaskQueueMutex);

                    // Wake-up condition: isStop or there is a global task or the worker has personal tasks or the worker is stopping
                    globalTaskQueueCondVar.wait(lock, [this, self] {
                        return isStop.load() || !globalTaskQueue.empty() || self->hasPersonalTasks() || self->isStopping();
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

                if (task.hasTask()) {
                    try {
                        task.function();
                    } catch (...) {
                    }
                    --activeTasks;
                }
            }
        }

        void createWorker() {
            std::unique_lock<std::shared_mutex> lock(workerMutex);
            
            neko::uint64 workerId = nextWorkerId++;
            WorkerInfo workerInfo(std::make_unique<std::thread>(&ThreadPool::workerThread, this, workerId), workerId);
            workers.push_back(std::move(workerInfo));
        }

        void createWorkerUnsafe() {
            neko::uint64 workerId = nextWorkerId++;
            WorkerInfo workerInfo(std::make_unique<std::thread>(&ThreadPool::workerThread, this, workerId), workerId);
            workers.push_back(std::move(workerInfo));
        }

    public:
        explicit ThreadPool(neko::uint64 threadCount = std::thread::hardware_concurrency()) noexcept {
            if (threadCount == 0) {
                threadCount = 1;
            }

            workers.reserve(threadCount);
            for (neko::uint64 i = 0; i < threadCount; ++i) {
                createWorker();
            }
        }

        ~ThreadPool() noexcept {
            stop();
        }

        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;
        ThreadPool(ThreadPool &&) = delete;
        ThreadPool &operator=(ThreadPool &&) = delete;

        // ===================
        // === Submit Task ===
        // ===================

        /**
         * @brief Submit a task with normal priority.
         * @param function The function to execute.
         * @param args The arguments to pass to the function.
         * @return A future representing the result of the task.
         * @throws ex::ProgramExit if the thread pool is stopped.
         * @throws ex::TaskRejected if the task is rejected.
         */
        template <typename F, typename... Args>
        auto submit(F &&function, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>> {
            return submitWithPriority(neko::Priority::Normal, std::forward<F>(function), std::forward<Args>(args)...);
        }

        /**
         * @brief Submit a task with a specific priority.
         * @param priority The priority of the task.
         * @param function The function to execute.
         * @param args The arguments to pass to the function.
         * @return A future representing the result of the task.
         * @throws ex::ProgramExit if the thread pool is stopped.
         * @throws ex::TaskRejected if the task is rejected.
         */
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
                std::unique_lock<std::shared_mutex> lock(globalTaskQueueMutex);

                if (globalTaskQueue.size() >= maxQueueSize.load()) {
                    throw ex::TaskRejected("Task queue is full");
                }

                TaskId taskId = nextTaskId++;
                globalTaskQueue.emplace([task]() { (*task)(); }, priority, taskId);
            }

            globalTaskQueueCondVar.notify_one();
            return result;
        }

        /**
         * @brief Submit a task to a specific worker thread.
         * @param workerId The ID of the worker thread.
         * @param function The function to execute.
         * @param args The arguments to pass to the function.
         * @return A future representing the result of the task.
         * @throws ex::OutOfRange if the worker thread is not found.
         */
        template <typename F, typename... Args>
        auto submitToWorker(neko::uint64 workerId, F &&function, Args &&...args)
            -> std::future<std::invoke_result_t<F, Args...>> {
            using ReturnType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

            if (isStop.load()) {
                throw ex::ProgramExit("Cannot submit tasks to stopped thread pool");
            }

            WorkerInfo *self = nullptr;
            {
                std::shared_lock<std::shared_mutex> lock(workerMutex);
                for (auto &worker : workers) {
                    if (worker.isActive() && workerId == worker.getId()) {
                        self = &worker;
                        break;
                    }
                }
            }

            if (!self)
                throw ex::OutOfRange("Worker not found with ID: " + std::to_string(workerId));

            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                [function, args...]() mutable -> ReturnType {
                    return std::invoke(std::forward<F>(function), std::forward<Args>(args)...);
                });

            std::future<ReturnType> result = task->get_future();
            TaskId taskId = ++nextTaskId;
            Task personalTask{[task]() { (*task)(); }, neko::Priority::Normal, taskId};
            self->postTask(personalTask);

            // Try to wake the specific worker (only the targeted worker will be woken unless there are global tasks)
            globalTaskQueueCondVar.notify_all();
            return result;
        }

        // ===================
        // ===== Control =====
        // ===================

        /**
         * @brief Wait for all global tasks to complete.
         *
         * Blocks until the global task queue is empty and there are no active tasks.
         * @note This does not account for any per-worker personal task queues.
         */
        void waitForCompletion() {
            while (true) {
                std::shared_lock<std::shared_mutex> lock(globalTaskQueueMutex);
                if (globalTaskQueue.empty() && activeTasks.load() == 0) {
                    return;
                }
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        /**
         * @brief Wait for all tasks to complete with a timeout.
         * @param timeout The duration to wait before giving up.
         * @return True if all tasks completed within the timeout, false otherwise.
         */
        template <typename Rep, typename Period>
        bool waitForCompletion(std::chrono::duration<Rep, Period> duration) {
            auto endTime = std::chrono::steady_clock::now() + duration;

            while (std::chrono::steady_clock::now() < endTime) {
                std::shared_lock<std::shared_mutex> lock(globalTaskQueueMutex);
                if (globalTaskQueue.empty() && activeTasks.load() == 0) {
                    return true;
                }
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
        }

        /**
         * @brief Stop the thread pool.
         * @param waitForTasks Whether to wait for all tasks to complete before stopping.
         */
        void stop(bool waitForTasks = true) {

            isStop.store(true);
            globalTaskQueueCondVar.notify_all();

            for (auto &worker : workers) {
                if (worker.isActive()) {
                    worker.cleanup(waitForTasks);
                }
            }
        }

        /**
         * @brief Set the thread count.
         * @param threadCount The new thread count.
         * @note If downsizing threads, running tasks will not be interrupted; excess threads will be reclaimed after completing their tasks.
         * @throws ex::ProgramExit if the thread pool is stopped.
         */
        void setThreadCount(neko::uint64 threadCount) {
            if (isStop.load()) {
                throw ex::ProgramExit("Cannot resize stopped thread pool");
            }

            std::unique_lock<std::shared_mutex> lock(workerMutex);
            if (threadCount == 0) {
                threadCount = 1;
            }

            if (threadCount == workers.size()) {
                return;
            }

            if (threadCount > workers.size()) {
                // Add more workers
                while (workers.size() < threadCount) {
                    createWorkerUnsafe();
                }
            } else if (threadCount < workers.size()) {
                // Remove excess workers

                while (workers.size() > threadCount) {
                    WorkerInfo &w = workers.back();
                    w.setShouldStop(true);
                    globalTaskQueueCondVar.notify_all();

                    w.cleanup(true);
                    workers.pop_back();
                }                
            }
        }

        /**
         * @brief Set the maximum queue size.
         * @param maxSize The maximum number of tasks allowed in the queue.
         */
        void setMaxQueueSize(neko::uint64 maxSize) noexcept {
            maxQueueSize.store(maxSize);
        }

        /**
         * @brief Get all available worker thread IDs.
         * @return A vector of worker IDs.
         */
        std::vector<neko::uint64> getWorkerIds() const {
            std::shared_lock<std::shared_mutex> lock(workerMutex);
            std::vector<neko::uint64> ids;
            for (const auto &w : workers) {
                ids.push_back(w.getId());
            }
            return ids;
        }

        /**
         * @brief Get the total number of worker threads.
         * @return The total number of worker threads.
         */
        neko::uint64 getThreadCount() const {
            std::shared_lock<std::shared_mutex> lock(workerMutex);
            return workers.size();
        }

        /**
         * @brief Get the total number of pending tasks.
         * @return The total number of pending tasks.
         */
        neko::uint64 getPendingTaskCount() const {
            std::shared_lock<std::shared_mutex> lock(globalTaskQueueMutex);
            return globalTaskQueue.size();
        }

        /**
         * @brief Get the maximum queue size.
         * @return The maximum queue size.
         */
        neko::uint64 getMaxQueueSize() const noexcept {
            return maxQueueSize.load();
        }

        /**
         * @brief Check if the task queue is full.
         * @return True if the queue is full, false otherwise.
         */
        bool isQueueFull() const {
            std::shared_lock<std::shared_mutex> lock(globalTaskQueueMutex);
            return globalTaskQueue.size() >= maxQueueSize.load();
        }

        /**
         * @brief Get the thread utilization as a percentage.
         * @return The thread utilization percentage.
         */
        double getThreadUtilization() const noexcept {
            auto totalThreads = workers.size();
            auto activeThreads = activeTasks.load();
            if (totalThreads == 0)
                return 0.0;
            return static_cast<double>(activeThreads) / totalThreads;
        }

        /**
         * @brief Get the current queue utilization as a percentage.
         * @return The queue utilization percentage.
         */
        double getQueueUtilization() const {
            std::shared_lock<std::shared_mutex> lock(globalTaskQueueMutex);
            auto maxSize = maxQueueSize.load();
            if (maxSize == 0)
                return 0.0;
            return static_cast<double>(globalTaskQueue.size()) / maxSize;
        }
    };

} // namespace neko::core::thread
