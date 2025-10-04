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

// === Thread Count Management Tests ===

TEST_F(ThreadPoolTest, SetThreadCountIncrease) {
    ThreadPool pool(2);
    EXPECT_EQ(pool.getThreadCount(), 2);
    
    // Increase thread count
    pool.setThreadCount(4);
    EXPECT_EQ(pool.getThreadCount(), 4);
    
    // Verify new worker IDs are created
    auto workerIds = pool.getWorkerIds();
    EXPECT_EQ(workerIds.size(), 4);
    
    // Test that all threads are functional
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 8; ++i) {
        futures.push_back(pool.submit([&counter]() {
            std::this_thread::sleep_for(10ms);
            counter.fetch_add(1);
        }));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(counter.load(), 8);
}

TEST_F(ThreadPoolTest, SetThreadCountDecrease) {
    ThreadPool pool(4);
    EXPECT_EQ(pool.getThreadCount(), 4);
    
    // Wait for threads to stabilize
    std::this_thread::sleep_for(10ms);
    
    // Submit some tasks to ensure threads are active
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 6; ++i) {
        futures.push_back(pool.submit([&counter]() {
            std::this_thread::sleep_for(20ms);
            counter.fetch_add(1);
        }));
    }
    
    std::this_thread::sleep_for(10ms); // Let tasks start
    
    // Decrease thread count
    pool.setThreadCount(2);
    EXPECT_EQ(pool.getThreadCount(), 2);
    
    // Verify worker IDs are correct
    auto workerIds = pool.getWorkerIds();
    EXPECT_EQ(workerIds.size(), 2);
    
    // Wait for all original tasks to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(counter.load(), 6);
    
    // Wait a moment for cleanup
    std::this_thread::sleep_for(10ms);
    
    // Verify remaining threads still work
    auto future = pool.submit([&counter]() {
        counter.fetch_add(10);
        return 42;
    });
    
    EXPECT_EQ(future.get(), 42);
    EXPECT_EQ(counter.load(), 16);
}

TEST_F(ThreadPoolTest, SetThreadCountSameValue) {
    ThreadPool pool(3);
    EXPECT_EQ(pool.getThreadCount(), 3);
    
    auto workerIds_before = pool.getWorkerIds();
    
    // Set to same value
    pool.setThreadCount(3);
    EXPECT_EQ(pool.getThreadCount(), 3);
    
    auto workerIds_after = pool.getWorkerIds();
    EXPECT_EQ(workerIds_before, workerIds_after);
    
    // Verify functionality is unaffected
    auto future = pool.submit([]() { return 123; });
    EXPECT_EQ(future.get(), 123);
}

TEST_F(ThreadPoolTest, SetThreadCountZero) {
    ThreadPool pool(2);
    
    // Wait a moment for threads to stabilize
    std::this_thread::sleep_for(10ms);
    
    // Setting to 0 should default to 1
    pool.setThreadCount(0);
    EXPECT_EQ(pool.getThreadCount(), 1);
    
    // Wait a moment for the resize to complete
    std::this_thread::sleep_for(20ms);
    
    // Verify it still works
    auto future = pool.submit([]() { return 456; });
    EXPECT_EQ(future.get(), 456);
}

TEST_F(ThreadPoolTest, SetThreadCountAfterStop) {
    ThreadPool pool(2);
    
    pool.stop();
    
    // Should throw exception when trying to resize stopped pool
    EXPECT_THROW(pool.setThreadCount(4), neko::ex::ProgramExit);
}

TEST_F(ThreadPoolTest, SetThreadCountStressTest) {
    ThreadPool pool(2);
    
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    // Submit initial tasks
    for (int i = 0; i < 4; ++i) {
        futures.push_back(pool.submit([&counter]() {
            std::this_thread::sleep_for(10ms);
            counter.fetch_add(1);
        }));
    }
    
    // Increase thread count
    pool.setThreadCount(4);
    
    // Submit more tasks
    for (int i = 0; i < 4; ++i) {
        futures.push_back(pool.submit([&counter]() {
            std::this_thread::sleep_for(5ms);
            counter.fetch_add(1);
        }));
    }
    
    // Wait for all tasks
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(counter.load(), 8);
    EXPECT_EQ(pool.getThreadCount(), 4);
}

TEST_F(ThreadPoolTest, SetThreadCountBasicResize) {
    ThreadPool pool(2);
    
    // Test upsize only
    pool.setThreadCount(4);
    EXPECT_EQ(pool.getThreadCount(), 4);
    
    // Test functionality after resize
    auto future = pool.submit([]() { return 42; });
    EXPECT_EQ(future.get(), 42);
}

TEST_F(ThreadPoolTest, SetThreadCountWithWorkerSpecificTasks) {
    ThreadPool pool(2);
    
    auto initial_ids = pool.getWorkerIds();
    EXPECT_EQ(initial_ids.size(), 2);
    
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    // Submit tasks to specific workers
    for (int i = 0; i < 4; ++i) {
        futures.push_back(pool.submitToWorker(initial_ids[i % 2], [&counter]() {
            std::this_thread::sleep_for(30ms);
            counter.fetch_add(1);
        }));
    }
    
    std::this_thread::sleep_for(10ms);
    
    // Increase thread count while worker-specific tasks are running
    pool.setThreadCount(4);
    EXPECT_EQ(pool.getThreadCount(), 4);
    
    auto new_ids = pool.getWorkerIds();
    EXPECT_EQ(new_ids.size(), 4);
    
    // Wait for original tasks
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(counter.load(), 4);
    
    // Test new workers
    futures.clear();
    for (size_t i = 0; i < new_ids.size(); ++i) {
        futures.push_back(pool.submitToWorker(new_ids[i], [&counter]() {
            counter.fetch_add(1);
        }));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(counter.load(), 8);
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

TEST_F(ThreadPoolTest, PriorityOrdering) {
    ThreadPool pool(1);  // Single thread to ensure sequential execution
    
    std::vector<neko::Priority> execution_order;
    std::mutex order_mutex;
    std::atomic<bool> blocker{true};
    
    // Submit a blocking task first
    auto blocker_future = pool.submit([&blocker]() {
        while (blocker.load()) {
            std::this_thread::sleep_for(1ms);
        }
    });
    
    std::this_thread::sleep_for(10ms); // Ensure blocker starts
    
    // Now submit tasks with different priorities
    auto low_future = pool.submitWithPriority(neko::Priority::Low, [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(neko::Priority::Low);
    });
    
    auto normal_future = pool.submitWithPriority(neko::Priority::Normal, [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(neko::Priority::Normal);
    });
    
    auto high_future = pool.submitWithPriority(neko::Priority::High, [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(neko::Priority::High);
    });
    
    std::this_thread::sleep_for(10ms); // Ensure all tasks are queued
    
    // Release the blocker
    blocker.store(false);
    
    // Wait for all tasks
    blocker_future.wait();
    high_future.wait();
    normal_future.wait();
    low_future.wait();
    
    // Verify execution order: High, Normal, Low
    ASSERT_EQ(execution_order.size(), 3);
    EXPECT_EQ(execution_order[0], neko::Priority::High);
    EXPECT_EQ(execution_order[1], neko::Priority::Normal);
    EXPECT_EQ(execution_order[2], neko::Priority::Low);
}

TEST_F(ThreadPoolTest, SamePriorityTaskOrdering) {
    ThreadPool pool(1);  // Single thread to ensure sequential execution
    
    std::vector<int> execution_order;
    std::mutex order_mutex;
    std::atomic<bool> blocker{true};
    
    // Submit a blocking task first
    auto blocker_future = pool.submit([&blocker]() {
        while (blocker.load()) {
            std::this_thread::sleep_for(1ms);
        }
    });
    
    std::this_thread::sleep_for(10ms); // Ensure blocker starts
    
    // Submit multiple tasks with same priority
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool.submitWithPriority(neko::Priority::Normal, [&, i]() {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(i);
        }));
    }
    
    std::this_thread::sleep_for(10ms); // Ensure all tasks are queued
    
    // Release the blocker
    blocker.store(false);
    
    // Wait for all tasks
    blocker_future.wait();
    for (auto& future : futures) {
        future.wait();
    }
    
    // Tasks with same priority should execute in FIFO order (first submitted first executed)
    ASSERT_EQ(execution_order.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(execution_order[i], i);
    }
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
    EXPECT_EQ(pool.getPendingTaskCount(), 0);
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

// === Worker Management Tests ===

TEST_F(ThreadPoolTest, GetWorkerIds) {
    ThreadPool pool(4);
    
    auto workerIds = pool.getWorkerIds();
    
    EXPECT_EQ(workerIds.size(), 4);
    
    // Verify IDs are unique
    std::set<neko::uint64> uniqueIds(workerIds.begin(), workerIds.end());
    EXPECT_EQ(uniqueIds.size(), 4);
    
    // Verify IDs are sequential starting from 0
    for (size_t i = 0; i < workerIds.size(); ++i) {
        EXPECT_EQ(workerIds[i], i);
    }
}

TEST_F(ThreadPoolTest, WorkerIdConsistency) {
    ThreadPool pool(2);
    
    auto ids_before = pool.getWorkerIds();
    EXPECT_EQ(ids_before.size(), 2);
    
    // Submit some tasks
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool.submit([]() {
            std::this_thread::sleep_for(10ms);
        }));
    }
    
    auto ids_during = pool.getWorkerIds();
    EXPECT_EQ(ids_during.size(), 2);
    
    for (auto& future : futures) {
        future.wait();
    }
    
    auto ids_after = pool.getWorkerIds();
    EXPECT_EQ(ids_after.size(), 2);
    
    // IDs should remain consistent
    EXPECT_EQ(ids_before, ids_during);
    EXPECT_EQ(ids_during, ids_after);
}

// === Queue Size Limit Tests ===

TEST_F(ThreadPoolTest, QueueFullRejection) {
    ThreadPool pool(1);  // Single thread
    
    pool.setMaxQueueSize(3);
    EXPECT_EQ(pool.getMaxQueueSize(), 3);
    
    std::atomic<bool> task_running{false};
    
    // Submit a long-running task to block the thread
    auto blocking_future = pool.submit([&task_running]() {
        task_running.store(true);
        std::this_thread::sleep_for(100ms);
    });
    
    // Wait for the blocking task to start
    while (!task_running.load()) {
        std::this_thread::sleep_for(1ms);
    }
    
    // Fill the queue
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 3; ++i) {
        futures.push_back(pool.submit([]() {
            std::this_thread::sleep_for(10ms);
        }));
    }
    
    // Next submission should fail
    EXPECT_THROW(
        pool.submit([]() {}),
        neko::ex::TaskRejected
    );
    
    blocking_future.wait();
    for (auto& future : futures) {
        future.wait();
    }
}

TEST_F(ThreadPoolTest, SetMaxQueueSizeDynamic) {
    ThreadPool pool(1);
    
    // Start with default size
    EXPECT_EQ(pool.getMaxQueueSize(), 100000);
    
    // Change to smaller size
    pool.setMaxQueueSize(5);
    EXPECT_EQ(pool.getMaxQueueSize(), 5);
    
    // Change to larger size
    pool.setMaxQueueSize(1000);
    EXPECT_EQ(pool.getMaxQueueSize(), 1000);
    
    // Change to 0 (special case)
    pool.setMaxQueueSize(0);
    EXPECT_EQ(pool.getMaxQueueSize(), 0);
    
    // With max size 0, queue utilization should be 0
    EXPECT_DOUBLE_EQ(pool.getQueueUtilization(), 0.0);
}

TEST_F(ThreadPoolTest, QueueFullWithPriorities) {
    ThreadPool pool(1);  // Single thread
    
    pool.setMaxQueueSize(2);
    std::atomic<bool> task_running{false};
    
    // Submit a blocking task
    auto blocking_future = pool.submit([&task_running]() {
        task_running.store(true);
        std::this_thread::sleep_for(100ms);
    });
    
    // Wait for blocker to start
    while (!task_running.load()) {
        std::this_thread::sleep_for(1ms);
    }
    
    // Fill queue with low priority tasks
    auto low1 = pool.submitWithPriority(neko::Priority::Low, []() {});
    auto low2 = pool.submitWithPriority(neko::Priority::Low, []() {});
    
    // Queue should be full now
    EXPECT_TRUE(pool.isQueueFull());
    
    // Next task should be rejected regardless of priority
    EXPECT_THROW(
        pool.submitWithPriority(neko::Priority::High, []() {}),
        neko::ex::TaskRejected
    );
    
    blocking_future.wait();
    low1.wait();
    low2.wait();
}

TEST_F(ThreadPoolTest, QueueUtilizationTracking) {
    ThreadPool pool(1);
    
    pool.setMaxQueueSize(10);
    
    // Initially empty
    EXPECT_DOUBLE_EQ(pool.getQueueUtilization(), 0.0);
    
    std::atomic<bool> task_running{false};
    
    // Block the thread
    auto blocking_future = pool.submit([&task_running]() {
        task_running.store(true);
        std::this_thread::sleep_for(100ms);
    });
    
    while (!task_running.load()) {
        std::this_thread::sleep_for(1ms);
    }
    
    // Add 5 tasks to queue
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool.submit([]() {
            std::this_thread::sleep_for(5ms);
        }));
    }
    
    std::this_thread::sleep_for(10ms);
    
    double utilization = pool.getQueueUtilization();
    EXPECT_GE(utilization, 0.0);
    EXPECT_LE(utilization, 1.0);
    // Should be around 0.5 (5 tasks out of 10 max)
    EXPECT_GT(utilization, 0.3);
    
    blocking_future.wait();
    for (auto& future : futures) {
        future.wait();
    }
}

// === Task Return Value Tests ===

TEST_F(ThreadPoolTest, ComplexReturnTypes) {
    ThreadPool pool(2);
    
    // Test with custom struct
    struct Result {
        int value;
        std::string message;
        
        bool operator==(const Result& other) const {
            return value == other.value && message == other.message;
        }
    };
    
    auto future = pool.submit([]() -> Result {
        return {42, "Success"};
    });
    
    Result result = future.get();
    EXPECT_EQ(result.value, 42);
    EXPECT_EQ(result.message, "Success");
}

TEST_F(ThreadPoolTest, TaskWithArguments) {
    ThreadPool pool(2);
    
    auto add_future = pool.submit([](int a, int b) {
        return a + b;
    }, 10, 32);
    
    auto concat_future = pool.submit([](const std::string& s1, const std::string& s2) {
        return s1 + s2;
    }, std::string("Hello "), std::string("World"));
    
    EXPECT_EQ(add_future.get(), 42);
    EXPECT_EQ(concat_future.get(), "Hello World");
}

// === Concurrent Stress Tests ===

TEST_F(ThreadPoolTest, HighConcurrencyStress) {
    ThreadPool pool(4);
    
    const int num_tasks = 100;
    std::atomic<int> counter{0};
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(pool.submit([&counter, i]() {
            std::this_thread::sleep_for(1ms);
            counter.fetch_add(1);
            return i;
        }));
    }
    
    // Verify all tasks complete
    int sum = 0;
    for (int i = 0; i < num_tasks; ++i) {
        EXPECT_EQ(futures[i].get(), i);
        sum += i;
    }
    
    EXPECT_EQ(counter.load(), num_tasks);
    EXPECT_EQ(sum, (num_tasks * (num_tasks - 1)) / 2);
}

TEST_F(ThreadPoolTest, MixedPriorityTasks) {
    ThreadPool pool(2);
    
    std::atomic<int> execution_count{0};
    std::vector<std::future<int>> futures;
    
    // Submit mix of priorities
    for (int i = 0; i < 10; ++i) {
        neko::Priority priority;
        if (i % 3 == 0) {
            priority = neko::Priority::High;
        } else if (i % 3 == 1) {
            priority = neko::Priority::Normal;
        } else {
            priority = neko::Priority::Low;
        }
        
        futures.push_back(pool.submitWithPriority(priority, [&execution_count]() {
            execution_count.fetch_add(1);
            std::this_thread::sleep_for(5ms);
            return execution_count.load();
        }));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(execution_count.load(), 10);
}

// === Thread Utilization Tests ===

TEST_F(ThreadPoolTest, ThreadUtilizationUnderLoad) {
    ThreadPool pool(4);
    
    // Submit enough tasks to keep all threads busy
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 8; ++i) {
        futures.push_back(pool.submit([]() {
            std::this_thread::sleep_for(50ms);
        }));
    }
    
    std::this_thread::sleep_for(20ms);
    
    // Should have high utilization
    double utilization = pool.getThreadUtilization();
    EXPECT_GE(utilization, 0.5);
    EXPECT_LE(utilization, 1.0);
    
    for (auto& future : futures) {
        future.wait();
    }
    
    // After completion, utilization should drop
    std::this_thread::sleep_for(10ms);
    EXPECT_LE(pool.getThreadUtilization(), 0.1);
}

TEST_F(ThreadPoolTest, ThreadUtilizationEdgeCases) {
    // Test with 0 active tasks
    ThreadPool pool(3);
    EXPECT_DOUBLE_EQ(pool.getThreadUtilization(), 0.0);
    
    // Test with partial utilization
    std::vector<std::future<void>> futures;
    futures.push_back(pool.submit([]() {
        std::this_thread::sleep_for(50ms);
    }));
    
    std::this_thread::sleep_for(10ms);
    
    double utilization = pool.getThreadUtilization();
    EXPECT_GE(utilization, 0.0);
    EXPECT_LE(utilization, 1.0);
    
    futures[0].wait();
}

TEST_F(ThreadPoolTest, QueueUtilizationDetailedTest) {
    ThreadPool pool(1);
    pool.setMaxQueueSize(10);
    
    std::atomic<bool> blocker{true};
    
    // Block the worker
    auto blocking_future = pool.submit([&blocker]() {
        while (blocker.load()) {
            std::this_thread::sleep_for(1ms);
        }
    });
    
    std::this_thread::sleep_for(10ms);
    
    // Add tasks incrementally and check utilization
    std::vector<std::future<void>> futures;
    
    // Add 3 tasks
    for (int i = 0; i < 3; ++i) {
        futures.push_back(pool.submit([]() {}));
    }
    std::this_thread::sleep_for(5ms);
    double util1 = pool.getQueueUtilization();
    EXPECT_GE(util1, 0.25);  // Should be around 0.3
    EXPECT_LE(util1, 0.35);
    
    // Add 2 more tasks (total 5)
    for (int i = 0; i < 2; ++i) {
        futures.push_back(pool.submit([]() {}));
    }
    std::this_thread::sleep_for(5ms);
    double util2 = pool.getQueueUtilization();
    EXPECT_GE(util2, 0.45);  // Should be around 0.5
    EXPECT_LE(util2, 0.55);
    
    // Release blocker and wait
    blocker.store(false);
    blocking_future.wait();
    for (auto& future : futures) {
        future.wait();
    }
    
    // Utilization should be 0 after completion
    EXPECT_DOUBLE_EQ(pool.getQueueUtilization(), 0.0);
}

TEST_F(ThreadPoolTest, GetPendingTaskCountAccuracy) {
    ThreadPool pool(1);
    
    std::atomic<bool> blocker{true};
    
    // Block the worker
    auto blocking_future = pool.submit([&blocker]() {
        while (blocker.load()) {
            std::this_thread::sleep_for(1ms);
        }
    });
    
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(pool.getPendingTaskCount(), 0);  // Blocking task is running, not pending
    
    // Add pending tasks
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool.submit([]() {}));
        std::this_thread::sleep_for(1ms);
        EXPECT_EQ(pool.getPendingTaskCount(), i + 1);
    }
    
    // Release blocker
    blocker.store(false);
    blocking_future.wait();
    
    // Wait for tasks to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(pool.getPendingTaskCount(), 0);
}

// === Submit To Specific Worker Tests ===

TEST_F(ThreadPoolTest, SubmitToWorkerBasic) {
    ThreadPool pool(4);
    
    auto workerIds = pool.getWorkerIds();
    ASSERT_GE(workerIds.size(), 2);
    
    std::atomic<int> counter{0};
    
    // Submit task to specific worker
    auto future1 = pool.submitToWorker(workerIds[0], [&counter]() {
        counter.fetch_add(1);
        return 42;
    });
    
    auto future2 = pool.submitToWorker(workerIds[1], [&counter]() {
        counter.fetch_add(10);
        return 100;
    });
    
    EXPECT_EQ(future1.get(), 42);
    EXPECT_EQ(future2.get(), 100);
    EXPECT_EQ(counter.load(), 11);
}

TEST_F(ThreadPoolTest, SubmitToWorkerWithArguments) {
    ThreadPool pool(3);
    
    auto workerIds = pool.getWorkerIds();
    ASSERT_GE(workerIds.size(), 1);
    
    auto future = pool.submitToWorker(workerIds[0], 
        [](int a, int b, const std::string& s) {
            return std::to_string(a + b) + s;
        }, 10, 32, std::string(" result"));
    
    EXPECT_EQ(future.get(), "42 result");
}

TEST_F(ThreadPoolTest, SubmitToWorkerMultipleTasks) {
    ThreadPool pool(2);
    
    auto workerIds = pool.getWorkerIds();
    ASSERT_GE(workerIds.size(), 1);
    
    std::atomic<int> execution_count{0};
    std::vector<std::future<int>> futures;
    
    // Submit multiple tasks to same worker
    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool.submitToWorker(workerIds[0], [&execution_count, i]() {
            std::this_thread::sleep_for(10ms);
            execution_count.fetch_add(1);
            return i;
        }));
    }
    
    // Verify all tasks complete
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(futures[i].get(), i);
    }
    
    EXPECT_EQ(execution_count.load(), 5);
}

TEST_F(ThreadPoolTest, SubmitToWorkerInvalidId) {
    ThreadPool pool(2);
    
    // Try to submit to non-existent worker
    EXPECT_THROW(
        pool.submitToWorker(999, []() { return 42; }),
        neko::ex::OutOfRange
    );
}

TEST_F(ThreadPoolTest, SubmitToWorkerAfterStop) {
    ThreadPool pool(2);
    
    auto workerIds = pool.getWorkerIds();
    ASSERT_GE(workerIds.size(), 1);
    
    pool.stop();
    
    EXPECT_THROW(
        pool.submitToWorker(workerIds[0], []() {}),
        neko::ex::ProgramExit
    );
}

TEST_F(ThreadPoolTest, SubmitToWorkerDistribution) {
    ThreadPool pool(3);
    
    auto workerIds = pool.getWorkerIds();
    ASSERT_EQ(workerIds.size(), 3);
    
    std::atomic<int> worker0_count{0};
    std::atomic<int> worker1_count{0};
    std::atomic<int> worker2_count{0};
    
    std::vector<std::future<void>> futures;
    
    // Distribute tasks to different workers
    for (int i = 0; i < 6; ++i) {
        neko::uint64 targetWorker = workerIds[i % 3];
        
        futures.push_back(pool.submitToWorker(targetWorker, [targetWorker, &workerIds, &worker0_count, &worker1_count, &worker2_count]() {
            std::this_thread::sleep_for(5ms);
            if (targetWorker == workerIds[0]) worker0_count.fetch_add(1);
            else if (targetWorker == workerIds[1]) worker1_count.fetch_add(1);
            else if (targetWorker == workerIds[2]) worker2_count.fetch_add(1);
        }));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    // Each worker should have executed 2 tasks
    EXPECT_EQ(worker0_count.load(), 2);
    EXPECT_EQ(worker1_count.load(), 2);
    EXPECT_EQ(worker2_count.load(), 2);
}

// === Wait For Completion With Timeout Tests ===

TEST_F(ThreadPoolTest, WaitForCompletionWithTimeoutSuccess) {
    ThreadPool pool(2);
    
    std::atomic<int> counter{0};
    
    // Submit quick tasks
    for (int i = 0; i < 3; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(20ms);
            counter.fetch_add(1);
        });
    }
    
    // Wait with generous timeout - should complete
    pool.waitForCompletion(500ms);
    
    EXPECT_EQ(counter.load(), 3);
    EXPECT_EQ(pool.getPendingTaskCount(), 0);
}

TEST_F(ThreadPoolTest, WaitForCompletionWithTimeoutExpires) {
    ThreadPool pool(1);  // Single thread
    
    std::atomic<int> counter{0};
    
    // Submit long-running tasks
    for (int i = 0; i < 3; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(100ms);
            counter.fetch_add(1);
        });
    }
    
    auto start = std::chrono::steady_clock::now();
    
    // Wait with short timeout - should expire before all tasks complete
    pool.waitForCompletion(50ms);
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should return after approximately 50ms
    EXPECT_GE(duration.count(), 45);
    EXPECT_LE(duration.count(), 100);
    
    // Not all tasks should be complete yet
    EXPECT_LT(counter.load(), 3);
    
    // Wait for remaining tasks
    pool.waitForCompletion();
    EXPECT_EQ(counter.load(), 3);
}

TEST_F(ThreadPoolTest, WaitForCompletionWithTimeoutEmptyPool) {
    ThreadPool pool(2);
    
    auto start = std::chrono::steady_clock::now();
    
    // Wait on empty pool - should return immediately
    pool.waitForCompletion(100ms);
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should return almost immediately
    EXPECT_LT(duration.count(), 20);
}

TEST_F(ThreadPoolTest, WaitForCompletionWithTimeoutDifferentDurations) {
    ThreadPool pool(2);
    
    // Test with seconds
    pool.submit([]() { std::this_thread::sleep_for(10ms); });
    pool.waitForCompletion(std::chrono::seconds(1));
    EXPECT_EQ(pool.getPendingTaskCount(), 0);
    
    // Test with microseconds
    pool.submit([]() { std::this_thread::sleep_for(10ms); });
    pool.waitForCompletion(std::chrono::microseconds(500000));
    EXPECT_EQ(pool.getPendingTaskCount(), 0);
    
    // Test with nanoseconds (very short)
    pool.submit([]() { std::this_thread::sleep_for(100ms); });
    pool.waitForCompletion(std::chrono::nanoseconds(1000000)); // 1ms
    // Should timeout quickly
    EXPECT_GT(pool.getPendingTaskCount(), 0);
    
    pool.waitForCompletion();
}

TEST_F(ThreadPoolTest, WaitForCompletionComparisonNoTimeout) {
    ThreadPool pool(2);
    
    std::atomic<int> counter{0};
    
    // Submit tasks
    for (int i = 0; i < 5; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(20ms);
            counter.fetch_add(1);
        });
    }
    
    auto start1 = std::chrono::steady_clock::now();
    pool.waitForCompletion();  // No timeout version
    auto end1 = std::chrono::steady_clock::now();
    
    EXPECT_EQ(counter.load(), 5);
    
    counter.store(0);
    
    // Submit same tasks
    for (int i = 0; i < 5; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(20ms);
            counter.fetch_add(1);
        });
    }
    
    auto start2 = std::chrono::steady_clock::now();
    pool.waitForCompletion(5000ms);  // With generous timeout
    auto end2 = std::chrono::steady_clock::now();
    
    EXPECT_EQ(counter.load(), 5);
    
    // Both should take similar time since all tasks complete
    auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);
    auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);
    
    EXPECT_LT(std::abs(duration1.count() - duration2.count()), 50);
}

// === Combined Feature Tests ===

TEST_F(ThreadPoolTest, SubmitToWorkerAndWaitWithTimeout) {
    ThreadPool pool(3);
    
    auto workerIds = pool.getWorkerIds();
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    // Submit tasks to specific workers
    for (int i = 0; i < 6; ++i) {
        futures.push_back(pool.submitToWorker(workerIds[i % 3], [&counter]() {
            std::this_thread::sleep_for(30ms);
            counter.fetch_add(1);
        }));
    }
    
    // Wait for futures to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(counter.load(), 6);
}

TEST_F(ThreadPoolTest, ComprehensiveFeatureIntegration) {
    ThreadPool pool(2);
    
    // Test initial state
    EXPECT_EQ(pool.getThreadCount(), 2);
    EXPECT_EQ(pool.getMaxQueueSize(), 100000);
    EXPECT_EQ(pool.getPendingTaskCount(), 0);
    EXPECT_DOUBLE_EQ(pool.getThreadUtilization(), 0.0);
    EXPECT_DOUBLE_EQ(pool.getQueueUtilization(), 0.0);
    EXPECT_FALSE(pool.isQueueFull());
    
    // Change thread count and queue size
    pool.setThreadCount(4);
    pool.setMaxQueueSize(20);
    
    EXPECT_EQ(pool.getThreadCount(), 4);
    EXPECT_EQ(pool.getMaxQueueSize(), 20);
    
    auto workerIds = pool.getWorkerIds();
    EXPECT_EQ(workerIds.size(), 4);
    
    // Submit mixed priority tasks
    std::atomic<int> execution_counter{0};
    std::vector<std::future<void>> futures;
    
    // High priority tasks
    for (int i = 0; i < 3; ++i) {
        futures.push_back(pool.submitWithPriority(neko::Priority::High, [&execution_counter]() {
            std::this_thread::sleep_for(20ms);
            execution_counter.fetch_add(1);
        }));
    }
    
    // Worker-specific tasks
    for (size_t i = 0; i < workerIds.size(); ++i) {
        futures.push_back(pool.submitToWorker(workerIds[i], [&execution_counter]() {
            std::this_thread::sleep_for(15ms);
            execution_counter.fetch_add(1);
        }));
    }
    
    // Normal priority tasks
    for (int i = 0; i < 3; ++i) {
        futures.push_back(pool.submit([&execution_counter]() {
            std::this_thread::sleep_for(10ms);
            execution_counter.fetch_add(1);
        }));
    }
    
    std::this_thread::sleep_for(10ms);
    
    // Check utilization during execution
    EXPECT_GT(pool.getThreadUtilization(), 0.0);
    EXPECT_GT(pool.getPendingTaskCount(), 0);
    EXPECT_GT(pool.getQueueUtilization(), 0.0);
    
    // Wait for completion with timeout
    bool completed = pool.waitForCompletion(2000ms);
    EXPECT_TRUE(completed);
    
    // Wait for all futures
    for (auto& future : futures) {
        future.wait();
    }
    
    // Verify final state
    EXPECT_EQ(execution_counter.load(), 10);  // 3 + 4 + 3 tasks
    EXPECT_EQ(pool.getPendingTaskCount(), 0);
    EXPECT_DOUBLE_EQ(pool.getQueueUtilization(), 0.0);
    
    // Test downsizing
    pool.setThreadCount(2);
    EXPECT_EQ(pool.getThreadCount(), 2);
    
    // Verify downsized pool still works
    auto final_future = pool.submit([]() { return 42; });
    EXPECT_EQ(final_future.get(), 42);
}

TEST_F(ThreadPoolTest, StressTestAllFeatures) {
    ThreadPool pool(1);
    
    // Start with small configuration
    pool.setMaxQueueSize(50);
    
    std::atomic<int> total_executed{0};
    std::vector<std::future<void>> all_futures;
    
    // Phase 1: Single thread, mixed priorities
    for (int i = 0; i < 10; ++i) {
        neko::Priority priority = (i % 3 == 0) ? neko::Priority::High :
                                 (i % 3 == 1) ? neko::Priority::Normal :
                                                neko::Priority::Low;
        all_futures.push_back(pool.submitWithPriority(priority, [&total_executed]() {
            std::this_thread::sleep_for(5ms);
            total_executed.fetch_add(1);
        }));
    }
    
    // Phase 2: Scale up threads
    pool.setThreadCount(4);
    auto worker_ids = pool.getWorkerIds();
    
    // Submit worker-specific tasks
    for (int i = 0; i < 20; ++i) {
        all_futures.push_back(pool.submitToWorker(worker_ids[i % worker_ids.size()], [&total_executed]() {
            std::this_thread::sleep_for(3ms);
            total_executed.fetch_add(1);
        }));
    }
    
    // Phase 3: Regular submissions with queue monitoring
    int batch_size = 15;
    for (int i = 0; i < batch_size; ++i) {
        if (!pool.isQueueFull()) {
            all_futures.push_back(pool.submit([&total_executed]() {
                std::this_thread::sleep_for(2ms);
                total_executed.fetch_add(1);
            }));
        }
    }
    
    // Phase 4: Scale down while tasks are running
    std::this_thread::sleep_for(10ms);
    pool.setThreadCount(2);
    
    // Wait for all tasks to complete
    bool completed = pool.waitForCompletion(3000ms);
    EXPECT_TRUE(completed);
    
    for (auto& future : all_futures) {
        future.wait();
    }
    
    // Verify all tasks executed
    EXPECT_EQ(total_executed.load(), all_futures.size());
    EXPECT_EQ(pool.getThreadCount(), 2);
    EXPECT_EQ(pool.getPendingTaskCount(), 0);
}

TEST_F(ThreadPoolTest, MixedSubmissionWithTimeout) {
    ThreadPool pool(3);
    
    auto workerIds = pool.getWorkerIds();
    std::atomic<int> global_tasks{0};
    std::atomic<int> worker_tasks{0};
    
    std::vector<std::future<void>> global_futures;
    std::vector<std::future<void>> worker_futures;
    
    // Mix of global and worker-specific tasks
    for (int i = 0; i < 4; ++i) {
        global_futures.push_back(pool.submit([&global_tasks]() {
            std::this_thread::sleep_for(20ms);
            global_tasks.fetch_add(1);
        }));
        
        worker_futures.push_back(pool.submitToWorker(workerIds[i % workerIds.size()], [&worker_tasks]() {
            std::this_thread::sleep_for(20ms);
            worker_tasks.fetch_add(1);
        }));
    }
    
    // Wait for all tasks via futures
    for (auto& future : global_futures) {
        future.wait();
    }
    for (auto& future : worker_futures) {
        future.wait();
    }
    
    EXPECT_EQ(global_tasks.load(), 4);
    EXPECT_EQ(worker_tasks.load(), 4);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
