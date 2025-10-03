/**
 * @brief Simplified Thread pool tests - Core functionality only
 * @file threadPool_test_simple.cpp
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

using namespace neko::core::thread;
using namespace std::chrono_literals;

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
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

TEST_F(ThreadPoolTest, ThreadCount) {
    ThreadPool pool1(0);  // Should default to 1
    EXPECT_EQ(pool1.getThreadCount(), 1);
    
    ThreadPool pool2(4);
    EXPECT_EQ(pool2.getThreadCount(), 4);
}

// === Priority Tests ===

TEST_F(ThreadPoolTest, PrioritySubmission) {
    ThreadPool pool(1);  // Single thread to ensure order
    
    std::atomic<int> counter{0};
    std::vector<int> execution_order;
    std::mutex order_mutex;
    
    // Submit with different priorities
    auto low_future = pool.submitWithPriority(neko::Priority::Low, [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(1);
        counter.fetch_add(1);
        return 1;
    });
    
    auto high_future = pool.submitWithPriority(neko::Priority::High, [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(2);
        counter.fetch_add(10);
        return 10;
    });
    
    low_future.wait();
    high_future.wait();
    
    EXPECT_EQ(counter.load(), 11);
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

TEST_F(ThreadPoolTest, WaitForCompletion) {
    ThreadPool pool(2);
    
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 3; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(20ms);
            counter.fetch_add(1);
        });
    }
    
    pool.waitForCompletion();
    EXPECT_EQ(counter.load(), 3);
    EXPECT_TRUE(pool.isEmpty());
}

// === Exception Handling ===

TEST_F(ThreadPoolTest, ExceptionInTask) {
    ThreadPool pool(2);
    
    auto future = pool.submit([]() -> int {
        throw std::runtime_error("Test exception");
        return 42;
    });
    
    EXPECT_THROW(future.get(), std::runtime_error);
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
    // With 3 threads and 20 tasks at 5ms each, ideal time is ~35ms
    // Allow for overhead and scheduling delays
    EXPECT_LT(duration.count(), 200);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
