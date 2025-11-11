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
#include <optional>

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
        neko::Priority priority{neko::Priority::Normal};
        TaskId id{0};

        Task() = default;
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
            return static_cast<bool>(function);
        }
    };

    class WorkerInfo {
    private:
        std::atomic<bool> shouldStop{false};
        std::thread thread;
        neko::uint64 id{0};

        std::queue<Task> personalTaskQueue;
        mutable std::shared_mutex personalTaskMutex;

    public:
        WorkerInfo() = default;
        explicit WorkerInfo(neko::uint64 i) : id(i) {}

        WorkerInfo(const WorkerInfo &) = delete;
        WorkerInfo &operator=(const WorkerInfo &) = delete;
        WorkerInfo(WorkerInfo &&) = delete;
        WorkerInfo &operator=(WorkerInfo &&) = delete;

        ~WorkerInfo() noexcept {
            if (isActive()) {
                cleanup();
            }
        }

        void addWorker(std::thread &&t) {
            thread = std::move(t);
        }

        void postTask(const Task &task) {
            std::lock_guard<std::shared_mutex> lock(personalTaskMutex);
            personalTaskQueue.push(task);
        }

        void postTask(Task &&task) {
            std::lock_guard<std::shared_mutex> lock(personalTaskMutex);
            personalTaskQueue.push(std::move(task));
        }

        std::optional<Task> fetchOnePersonal() {
            std::lock_guard<std::shared_mutex> lk(personalTaskMutex);
            if (personalTaskQueue.empty()) {
                return std::nullopt;
            }
            Task task = std::move(personalTaskQueue.front());
            personalTaskQueue.pop();
            return task;
        }

        void tryRunAllPersonalTasks() {
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
            return thread.joinable();
        }

        void setShouldStop(bool value) noexcept {
            shouldStop.store(value, std::memory_order_release);
        }

        void cleanup() noexcept {
            setShouldStop(true);

            if (!isActive()) {
                return;
            }

            try {
                thread.join();
            } catch (const std::system_error &) {
                return;
            }
        }
    };

    class ThreadPool {
    private:
        // Worker and global task queue
        std::vector<std::unique_ptr<WorkerInfo>> workers;
        std::priority_queue<Task> globalTaskQueue;

        mutable std::shared_mutex workerMutex;
        mutable std::shared_mutex globalTaskQueueMutex;
        std::condition_variable_any globalTaskQueueCondVar;

        // Stop flag and task counters
        std::atomic<bool> stopping{false};

        std::atomic<TaskId> nextTaskId{0};
        std::atomic<neko::uint64> nextWorkerId{0};

        std::atomic<neko::uint64> globalActiveTasks{0};
        std::atomic<neko::uint64> maxQueueSize{100000};

        /**
         * @brief Worker thread function that processes tasks.
         */
        static void workerLoop(ThreadPool *pool, WorkerInfo *self) {

            for (;;) {

                // Check for personal tasks first (outside any lock)
                if (self->hasPersonalTasks()) {
                    self->tryRunAllPersonalTasks();
                    continue;
                }

                // Check if stopping before waiting
                if (self->isStopping()) {
                    return;
                }

                Task gTask;
                bool hasGlobalTask = false;

                {
                    std::unique_lock<std::shared_mutex> lk(pool->globalTaskQueueMutex);

                    // Wait for work: global tasks OR stopping signal OR personal tasks
                    pool->globalTaskQueueCondVar.wait(lk, [&] {
                        return pool->stopping.load(std::memory_order_acquire) || 
                               self->isStopping() || 
                               !pool->globalTaskQueue.empty() ||
                               self->hasPersonalTasks();
                    });

                    // Check personal tasks first after waking up
                    if (self->hasPersonalTasks()) {
                        // Release lock and handle personal tasks
                        lk.unlock();
                        continue;
                    }

                    // If stopping and no global tasks, exit
                    if ((pool->stopping.load(std::memory_order_acquire) || self->isStopping()) && 
                        pool->globalTaskQueue.empty()) {
                        return;
                    }

                    // Try to get a global task
                    if (!pool->globalTaskQueue.empty()) {
                        const Task &top = pool->globalTaskQueue.top();
                        gTask = top; // Copy (Task is lightweight, function shares small object)
                        pool->globalTaskQueue.pop();
                        hasGlobalTask = true;
                        pool->globalActiveTasks.fetch_add(1, std::memory_order_acq_rel);
                    }
                }

                // Execute the global task outside the lock
                if (hasGlobalTask && gTask.hasTask()) {
                    try {
                        gTask.function();
                    } catch (...) {
                    }
                    pool->globalActiveTasks.fetch_sub(1, std::memory_order_acq_rel);
                }
            }
        }

        void addWorker() {
            std::unique_lock<std::shared_mutex> lock(workerMutex);

            auto id = nextWorkerId.fetch_add(1, std::memory_order_acq_rel);
            auto w = std::make_unique<WorkerInfo>(id);
            WorkerInfo *raw = w.get();
            raw->addWorker(std::thread(&ThreadPool::workerLoop, this, raw));
            workers.push_back(std::move(w));
        }

        void addWorkerUnsafe() {
            auto id = nextWorkerId.fetch_add(1, std::memory_order_acq_rel);
            auto w = std::make_unique<WorkerInfo>(id);
            WorkerInfo *raw = w.get();
            raw->addWorker(std::thread(&ThreadPool::workerLoop, this, raw));
            workers.push_back(std::move(w));
        }

    public:
        explicit ThreadPool(neko::uint64 threadCount = std::thread::hardware_concurrency()) noexcept {
            if (threadCount == 0) {
                threadCount = 1;
            }

            workers.reserve(threadCount);
            for (neko::uint64 i = 0; i < threadCount; ++i) {
                addWorkerUnsafe();
            }
        }

        ~ThreadPool() noexcept {
            try {
                stop();
            } catch (...) {
                // do nothing
            }
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

            if (stopping.load()) {
                throw ex::ProgramExit("Cannot submit tasks to stopped thread pool");
            }

            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                [fn = std::forward<F>(function), ... capArgs = std::forward<Args>(args)]() mutable -> ReturnType {
                    return std::invoke(std::move(fn), std::move(capArgs)...);
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

            if (stopping.load()) {
                throw ex::ProgramExit("Cannot submit tasks to stopped thread pool");
            }

            WorkerInfo *self = nullptr;
            {
                std::shared_lock<std::shared_mutex> lk(workerMutex);
                for (auto &w : workers) {
                    if (w->getId() == workerId) {
                        self = w.get();
                        break;
                    }
                }
            }

            if (!self)
                throw ex::OutOfRange("Worker not found with ID: " + std::to_string(workerId));

            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                [fn = std::forward<F>(function), ... capArgs = std::forward<Args>(args)]() mutable -> ReturnType {
                    return std::invoke(std::move(fn), std::move(capArgs)...);
                });

            TaskId taskId = nextTaskId.fetch_add(1, std::memory_order_acq_rel);
            Task personalTask{[task]() { (*task)(); }, neko::Priority::Normal, taskId};
            {
                self->postTask(std::move(personalTask));
            }

            // Try to wake the specific worker (only the targeted worker will be woken unless there are global tasks)
            globalTaskQueueCondVar.notify_all();
            return task->get_future();
        }

        // ===================
        // ===== Control =====
        // ===================

        /**
         * @brief Wait for all global tasks to complete.
         *
         * Blocks until the global task queue is empty and there are no active tasks.
         */
        void waitForGlobalTasks() {
            while (true) {
                std::shared_lock<std::shared_mutex> lock(globalTaskQueueMutex);
                if (globalTaskQueue.empty() && globalActiveTasks.load() == 0) {
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
        bool waitForGlobalTasks(std::chrono::duration<Rep, Period> duration) {
            auto endTime = std::chrono::steady_clock::now() + duration;

            while (std::chrono::steady_clock::now() < endTime) {
                std::shared_lock<std::shared_mutex> lock(globalTaskQueueMutex);
                if (globalTaskQueue.empty() && globalActiveTasks.load() == 0) {
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
            // Check if already stopping to avoid double-stop
            bool expected = false;
            if (!stopping.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                // Already stopping or stopped
                return;
            }

            if (waitForTasks) {
                waitForGlobalTasks();
            }

            {
                std::lock_guard<std::shared_mutex> lk(workerMutex);
                for (auto &w : workers) {
                    w->setShouldStop(true);
                }
            }

            globalTaskQueueCondVar.notify_all();

            for (auto &worker : workers) {
                worker->cleanup();
            }
        }

        /**
         * @brief Set the thread count.
         * @param newThreadCount The new thread count.
         * @note If downsizing threads, running tasks will not be interrupted; excess threads will be reclaimed after completing their tasks.
         * @throws ex::ProgramExit if the thread pool is stopped.
         */
        void setThreadCount(neko::uint64 newThreadCount) {
            if (stopping.load()) {
                throw ex::ProgramExit("Cannot resize stopped thread pool");
            }

            if (newThreadCount == 0) {
                newThreadCount = 1;
            }

            std::vector<std::unique_ptr<WorkerInfo>> removed;

            {
                std::unique_lock<std::shared_mutex> lock(workerMutex);
                
                if (newThreadCount == workers.size()) {
                    return;
                }

                if (newThreadCount > workers.size()) {
                    while (workers.size() < newThreadCount) {
                        addWorkerUnsafe();
                    }
                } else {
                    while (workers.size() > newThreadCount) {
                        removed.push_back(std::move(workers.back()));
                        workers.pop_back();
                    }
                }
            }

            // For removed workers: request stop, wake them, and join
            if (!removed.empty()) {
                for (auto &w : removed) {
                    w->setShouldStop(true);
                }
                globalTaskQueueCondVar.notify_all();
                for (auto &w : removed) {
                    w->cleanup();
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
                ids.push_back(w->getId());
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
            auto activeThreads = globalActiveTasks.load();
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
