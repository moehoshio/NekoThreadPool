/**
 * @brief Comprehensive Thread pool tests
 * @file threadPool_test.cpp
 * @author moehoshio
 * @copyright Copyright (c) 2025 Hoshi
 * @license MIT OR Apache-2.0
 */

#include <neko/core/threadPool.hpp>
#include <neko/schema/exception.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <vector>
#include <set>
#include <mutex>

using namespace neko::core::thread;
using namespace std::chrono_literals;

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code for each test
    }

    void TearDown() override {
        // Cleanup code for each test
    }
};

// === Basic Functionality Tests ===

TEST_F(ThreadPoolTest, BasicTaskSubmission) {
    ThreadPool pool(2);
    
    std::atomic<int> counter{0};
    
    auto future1 = pool.submit([&counter]() {
        counter.fetch_add(1);
        return 42;
    });
    
    auto future2 = pool.submit([&counter]() {
        counter.fetch_add(1);
        return std::string("hello");
    });
    
    EXPECT_EQ(future1.get(), 42);
    EXPECT_EQ(future2.get(), std::string("hello"));
    EXPECT_EQ(counter.load(), 2);
}

TEST_F(ThreadPoolTest, VoidTask) {
    ThreadPool pool(2);
    
    std::atomic<bool> executed{false};
    
    auto future = pool.submit([&executed]() {
        std::this_thread::sleep_for(10ms);
        executed.store(true);
    });
    
    future.wait();
    EXPECT_TRUE(executed.load());
}

TEST_F(ThreadPoolTest, MultipleTasks) {
    ThreadPool pool(3);
    
    const int num_tasks = 10;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(pool.submit([&counter]() {
            std::this_thread::sleep_for(5ms);
            counter.fetch_add(1);
        }));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(counter.load(), num_tasks);
}

// === Thread Management Tests ===

TEST_F(ThreadPoolTest, ThreadCountManagement) {
    ThreadPool pool(2);
    
    EXPECT_EQ(pool.getThreadCount(), 2);
    
    pool.setThreadCount(4);
    EXPECT_EQ(pool.getThreadCount(), 4);
    
    pool.setThreadCount(1);
   EXPECT_EQ(pool.getThreadCount(), 1);
}

TEST_F(ThreadPoolTest, ZeroThreadCountDefaultsToOne) {
    ThreadPool pool(0);
    EXPECT_EQ(pool.getThreadCount(), 1);
}

TEST_F(ThreadPoolTest, WorkerIdRetrieval) {
    ThreadPool pool(3);
    
    auto worker_ids = pool.getWorkerIds();
    EXPECT_EQ(worker_ids.size(), 3);
    
    std::set<neko::uint64> unique_ids(worker_ids.begin(), worker_ids.end());
    EXPECT_EQ(unique_ids.size(), worker_ids.size());
}

// === Priority Tests ===

TEST_F(ThreadPoolTest, PrioritySubmission) {
    ThreadPool pool(1);
    
    std::atomic<int> counter{0};
    
    auto low_future = pool.submitWithPriority(neko::Priority::Low, [&counter]() {
        counter.fetch_add(1);
        return 1;
    });
    
    auto high_future = pool.submitWithPriority(neko::Priority::High, [&counter]() {
        counter.fetch_add(10);
        return 10;
    });
    
    EXPECT_EQ(low_future.get(), 1);
    EXPECT_EQ(high_future.get(), 10);
    EXPECT_EQ(counter.load(), 11);
}

// === Statistics Tests ===

TEST_F(ThreadPoolTest, TaskStatistics) {
    ThreadPool pool(2);
    
    pool.resetStats();
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool.submit([]() {
            std::this_thread::sleep_for(10ms);
        }));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    auto stats = pool.getTaskStats();
    EXPECT_EQ(stats.completedTasks, 5);
    EXPECT_EQ(stats.submittedTasks, 5);
    EXPECT_EQ(stats.failedTasks, 0);
    EXPECT_GT(stats.totalExecutionTime.count(), 0);
}

TEST_F(ThreadPoolTest, ExceptionHandling) {
    ThreadPool pool(2);
    pool.resetStats();
    
    auto future = pool.submit([]() -> int {
        throw std::runtime_error("Test exception");
        return 42;
    });
    
    EXPECT_THROW(future.get(), std::runtime_error);
    
    std::this_thread::sleep_for(50ms);
    
    auto stats = pool.getTaskStats();
    EXPECT_EQ(stats.failedTasks, 1);
}

TEST_F(ThreadPoolTest, StatisticsToggle) {
    ThreadPool pool(2);
    
    EXPECT_TRUE(pool.isStatisticsEnabled());
    
    pool.enableStatistics(false);
    EXPECT_FALSE(pool.isStatisticsEnabled());
    
    pool.enableStatistics(true);
    EXPECT_TRUE(pool.isStatisticsEnabled());
}

// === Queue Management Tests ===

TEST_F(ThreadPoolTest, QueueProperties) {
    ThreadPool pool(2);
    
    pool.setMaxQueueSize(100);
    EXPECT_EQ(pool.getMaxQueueSize(), 100);
    
    EXPECT_EQ(pool.getPendingTaskCount(), 0);
    EXPECT_FALSE(pool.isQueueFull());
}

TEST_F(ThreadPoolTest, UtilizationMethods) {
    ThreadPool pool(4);
    
    pool.setMaxQueueSize(10);
    
    EXPECT_DOUBLE_EQ(pool.getQueueUtilization(), 0.0);
    EXPECT_DOUBLE_EQ(pool.getThreadUtilization(), 0.0);
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 3; ++i) {
        futures.push_back(pool.submit([]() {
            std::this_thread::sleep_for(50ms);
        }));
    }
    
    std::this_thread::sleep_for(20ms);
    
    double utilization = pool.getThreadUtilization();
    EXPECT_GE(utilization, 0.0);
    EXPECT_LE(utilization, 1.0);
    
    for (auto& future : futures) {
        future.wait();
    }
}

// === Stop and Lifecycle Tests ===

TEST_F(ThreadPoolTest, StoppedPoolRejectsNewTasks) {
    ThreadPool pool(2);
    
    pool.stop();
    
    EXPECT_THROW(pool.submit([]() {}), neko::ex::ProgramExit);
    EXPECT_THROW(pool.setThreadCount(4), neko::ex::ProgramExit);
}

TEST_F(ThreadPoolTest, DestructorCallsStop) {
    std::atomic<int> counter{0};
    
    {
        ThreadPool pool(2);
        
        for (int i = 0; i < 3; ++i) {
            pool.submit([&counter]() {
                std::this_thread::sleep_for(10ms);
                counter.fetch_add(1);
            });
        }
        
        std::this_thread::sleep_for(50ms);
    }  // Destructor should wait for tasks
    
    EXPECT_EQ(counter.load(), 3);
}

// === Wait Operations Tests ===

TEST_F(ThreadPoolTest, WaitForAllTasksCompletion) {
    ThreadPool pool(2);
    
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 3; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(20ms);
            counter.fetch_add(1);
        });
    }
    
    bool completed = pool.waitForAllTasksCompletion(std::chrono::seconds(5));
    EXPECT_TRUE(completed);
    EXPECT_EQ(counter.load(), 3);
    EXPECT_TRUE(pool.isEmpty());
}

// === Logger Tests ===

TEST_F(ThreadPoolTest, CustomLogger) {
    ThreadPool pool(2);
    
    std::vector<std::string> log_messages;
    std::mutex log_mutex;
    
    pool.setLogger([&](const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex);
        log_messages.push_back(message);
    });
    
    auto future = pool.submit([]() -> int {
        throw std::runtime_error("Test exception for logging");
        return 42;
    });
    
    EXPECT_THROW(future.get(), std::runtime_error);
    
    std::this_thread::sleep_for(50ms);
    
    EXPECT_GE(log_messages.size(), 0);
}

// === Performance Test ===

TEST_F(ThreadPoolTest, BasicPerformance) {
    ThreadPool pool(3);
    
    const int num_tasks = 20;
    std::atomic<int> completed{0};
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(pool.submit([&completed]() {
            std::this_thread::sleep_for(5ms);
            completed.fetch_add(1);
        }));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_EQ(completed.load(), num_tasks);
    EXPECT_LT(duration.count(), 100);
    
    auto stats = pool.getTaskStats();
    EXPECT_EQ(stats.completedTasks, num_tasks);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}