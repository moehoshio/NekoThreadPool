/**
 * @brief Neko Thread Pool Tests
 * @file threadPool_test.cpp
 * @author moehoshio
 * @copyright Copyright (c) 2025 Hoshi
 * @license MIT OR Apache-2.0
 */

#include <neko/core/threadPool.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <numeric>
#include <thread>
#include <vector>

using namespace neko::core::thread;

// ==========================================
// === Basic Functionality Tests ===
// ==========================================

TEST(ThreadPoolTest, BasicConstruction) {
    EXPECT_NO_THROW({
        ThreadPool pool(4);
    });
}

TEST(ThreadPoolTest, DefaultConstruction) {
    EXPECT_NO_THROW({
        ThreadPool pool;
    });
}

TEST(ThreadPoolTest, ZeroThreadConstruction) {
    ThreadPool pool(0);
    EXPECT_GE(pool.getThreadCount(), 1u);
}

// ==========================================
// === Task Submission Tests ===
// ==========================================

TEST(ThreadPoolTest, SubmitSimpleTask) {
    ThreadPool pool(2);
    std::atomic<bool> taskExecuted{false};

    auto future = pool.submit([&taskExecuted]() {
        taskExecuted = true;
    });

    future.wait();
    EXPECT_TRUE(taskExecuted.load());
}

TEST(ThreadPoolTest, SubmitTaskWithReturnValue) {
    ThreadPool pool(2);

    auto future = pool.submit([]() {
        return 42;
    });

    EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, SubmitTaskWithParameters) {
    ThreadPool pool(2);

    auto add = [](int a, int b) {
        return a + b;
    };

    auto future = pool.submit(add, 10, 20);
    EXPECT_EQ(future.get(), 30);
}

TEST(ThreadPoolTest, SubmitMultipleTasks) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.submit([&counter]() {
            counter++;
        }));
    }

    for (auto &future : futures) {
        future.wait();
    }

    EXPECT_EQ(counter.load(), 100);
}

TEST(ThreadPoolTest, SubmitTaskAfterStop) {
    ThreadPool pool(2);
    pool.stop();

    EXPECT_THROW({ pool.submit([]() {}); }, neko::ex::ProgramExit);
}

// ==========================================
// === Priority Tests ===
// ==========================================

TEST(ThreadPoolTest, TaskPriority) {
    ThreadPool pool(1);
    std::vector<int> executionOrder;
    std::mutex orderMutex;

    // Submit a long task first to block the worker
    auto blockingFuture = pool.submit([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    });

    // Wait a bit to ensure the blocking task starts
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Now submit tasks with different priorities
    auto lowFuture = pool.submitWithPriority(neko::Priority::Low, [&]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(3);
    });

    auto highFuture = pool.submitWithPriority(neko::Priority::High, [&]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(1);
    });

    auto normalFuture = pool.submitWithPriority(neko::Priority::Normal, [&]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(2);
    });

    blockingFuture.wait();
    highFuture.wait();
    normalFuture.wait();
    lowFuture.wait();

    EXPECT_EQ(executionOrder.size(), 3u);
    EXPECT_EQ(executionOrder[0], 1); // High priority first
    EXPECT_EQ(executionOrder[1], 2); // Normal priority second
    EXPECT_EQ(executionOrder[2], 3); // Low priority last
}

// ==========================================
// === Personal Task Queue Tests ===
// ==========================================

TEST(ThreadPoolTest, SubmitToWorker) {
    ThreadPool pool(4);
    auto workerIds = pool.getWorkerIds();
    ASSERT_FALSE(workerIds.empty());

    std::atomic<bool> taskExecuted{false};
    auto future = pool.submitToWorker(workerIds[0], [&taskExecuted]() {
        taskExecuted = true;
    });

    future.wait();
    EXPECT_TRUE(taskExecuted.load());
}

TEST(ThreadPoolTest, SubmitToWorkerWithReturnValue) {
    ThreadPool pool(4);
    auto workerIds = pool.getWorkerIds();
    ASSERT_FALSE(workerIds.empty());

    auto future = pool.submitToWorker(workerIds[0], []() {
        return 123;
    });

    EXPECT_EQ(future.get(), 123);
}

TEST(ThreadPoolTest, SubmitToInvalidWorker) {
    ThreadPool pool(2);

    EXPECT_THROW({ pool.submitToWorker(9999, []() {}); }, neko::ex::OutOfRange);
}

TEST(ThreadPoolTest, SubmitToMultipleWorkersInParallel) {
    ThreadPool pool(8);
    auto workerIds = pool.getWorkerIds();
    ASSERT_FALSE(workerIds.empty());

    std::vector<std::future<std::thread::id>> futures;
    
    // Submit tasks to all workers in parallel
    for (auto workerId : workerIds) {
        futures.push_back(pool.submitToWorker(workerId, []() {
            return std::this_thread::get_id();
        }));
    }
    
    // Verify all tasks complete successfully
    for (auto& future : futures) {
        auto threadId = future.get();
        EXPECT_NE(threadId, std::thread::id());
    }
}

// ==========================================
// === Wait For Completion Tests ===
// ==========================================

TEST(ThreadPoolTest, WaitForGlobalTasks) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};

    for (int i = 0; i < 50; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            counter++;
        });
    }

    pool.waitForGlobalTasks();
    EXPECT_EQ(counter.load(), 50);
}

TEST(ThreadPoolTest, WaitForGlobalTasksWithTimeout) {
    ThreadPool pool(2);

    // Submit quick tasks
    for (int i = 0; i < 10; ++i) {
        pool.submit([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        });
    }

    bool completed = pool.waitForGlobalTasks(std::chrono::seconds(5));
    EXPECT_TRUE(completed);
}

TEST(ThreadPoolTest, WaitForGlobalTasksTimeout) {
    ThreadPool pool(1);

    // Submit a long task
    auto future = pool.submit([]() {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    });

    bool completed = pool.waitForGlobalTasks(std::chrono::milliseconds(100));
    EXPECT_FALSE(completed);

    // Stop the pool to cancel the long-running task before destruction
    pool.stop(false);
}

// ==========================================
// === Thread Count Management Tests ===
// ==========================================

TEST(ThreadPoolTest, GetThreadCount) {
    ThreadPool pool(4);
    EXPECT_EQ(pool.getThreadCount(), 4u);
}

TEST(ThreadPoolTest, SetThreadCountIncrease) {
    ThreadPool pool(2);
    EXPECT_EQ(pool.getThreadCount(), 2u);

    pool.setThreadCount(4);
    EXPECT_EQ(pool.getThreadCount(), 4u);
}

TEST(ThreadPoolTest, SetThreadCountDecrease) {
    ThreadPool pool(4);
    EXPECT_EQ(pool.getThreadCount(), 4u);

    pool.setThreadCount(2);
    EXPECT_EQ(pool.getThreadCount(), 2u);
}

TEST(ThreadPoolTest, SetThreadCountZero) {
    ThreadPool pool(4);
    pool.setThreadCount(0);
    EXPECT_GE(pool.getThreadCount(), 1u);
}

TEST(ThreadPoolTest, SetThreadCountAfterStop) {
    ThreadPool pool(4);
    pool.stop();

    EXPECT_THROW({ pool.setThreadCount(2); }, neko::ex::ProgramExit);
}

// ==========================================
// === Queue Management Tests ===
// ==========================================

TEST(ThreadPoolTest, GetMaxQueueSize) {
    ThreadPool pool(2);
    EXPECT_GT(pool.getMaxQueueSize(), 0u);
}

TEST(ThreadPoolTest, SetMaxQueueSize) {
    ThreadPool pool(2);
    pool.setMaxQueueSize(50);
    EXPECT_EQ(pool.getMaxQueueSize(), 50u);
}

TEST(ThreadPoolTest, QueueFullRejection) {
    ThreadPool pool(1);
    pool.setMaxQueueSize(5);

    // Block the worker with a long task
    auto blockingFuture = pool.submit([]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    });

    // Wait a bit to ensure the blocking task starts
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Fill the queue
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool.submit([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }));
    }

    // This should throw because queue is full
    EXPECT_THROW({ pool.submit([]() {}); }, neko::ex::TaskRejected);

    // Wait for tasks to complete before destruction
    blockingFuture.wait();
    for (auto &f : futures) {
        f.wait();
    }
}

TEST(ThreadPoolTest, GetPendingTaskCount) {
    ThreadPool pool(1);

    // Block the worker
    auto blockingFuture = pool.submit([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Add pending tasks
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool.submit([]() {}));
    }

    EXPECT_GT(pool.getPendingTaskCount(), 0u);

    // Wait for all tasks to complete before destruction
    blockingFuture.wait();
    for (auto &f : futures) {
        f.wait();
    }
}

TEST(ThreadPoolTest, IsQueueFull) {
    ThreadPool pool(1);
    pool.setMaxQueueSize(3);

    // Block the worker
    auto blockingFuture = pool.submit([]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_FALSE(pool.isQueueFull());

    // Fill the queue
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 3; ++i) {
        futures.push_back(pool.submit([]() {}));
    }

    EXPECT_TRUE(pool.isQueueFull());

    // Wait for all tasks to complete before destruction
    blockingFuture.wait();
    for (auto &f : futures) {
        f.wait();
    }
}

// ==========================================
// === Statistics Tests ===
// ==========================================

TEST(ThreadPoolTest, GetWorkerIds) {
    ThreadPool pool(4);
    auto workerIds = pool.getWorkerIds();
    EXPECT_EQ(workerIds.size(), 4u);
}

TEST(ThreadPoolTest, GetThreadUtilization) {
    ThreadPool pool(4);
    auto utilization = pool.getThreadUtilization();
    EXPECT_GE(utilization, 0.0);
    EXPECT_LE(utilization, 1.0);
}

TEST(ThreadPoolTest, GetQueueUtilization) {
    ThreadPool pool(2);
    auto utilization = pool.getQueueUtilization();
    EXPECT_GE(utilization, 0.0);
    EXPECT_LE(utilization, 1.0);
}

TEST(ThreadPoolTest, ThreadUtilizationWithLoad) {
    ThreadPool pool(4);

    // Submit tasks to create load
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 8; ++i) {
        futures.push_back(pool.submit([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto utilization = pool.getThreadUtilization();

    // All threads should be busy
    EXPECT_GT(utilization, 0.0);

    // Wait for all tasks to complete before destruction
    for (auto &f : futures) {
        f.wait();
    }
}

// ==========================================
// === Stop and Cleanup Tests ===
// ==========================================

TEST(ThreadPoolTest, StopWithWait) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};

    for (int i = 0; i < 10; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            counter++;
        });
    }

    pool.stop(true);
    EXPECT_EQ(counter.load(), 10);
}

TEST(ThreadPoolTest, StopWithoutWait) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            counter++;
        });
    }

    pool.stop(false);
    // Not all tasks may complete
    EXPECT_LE(counter.load(), 100);
}

// ==========================================
// === Exception Handling Tests ===
// ==========================================

TEST(ThreadPoolTest, TaskWithException) {
    ThreadPool pool(2);

    auto future = pool.submit([]() {
        throw std::runtime_error("Test exception");
    });

    EXPECT_THROW({ future.get(); }, std::runtime_error);
}

TEST(ThreadPoolTest, MultipleTasksWithExceptions) {
    ThreadPool pool(4);
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.submit([i]() {
            if (i % 2 == 0) {
                throw std::runtime_error("Even task exception");
            }
        }));
    }

    int exceptionCount = 0;
    for (auto &future : futures) {
        try {
            future.get();
        } catch (const std::runtime_error &) {
            exceptionCount++;
        }
    }

    EXPECT_EQ(exceptionCount, 5);
}

// ==========================================
// === Stress Tests ===
// ==========================================

TEST(ThreadPoolTest, HighLoadSubmission) {
    ThreadPool pool(8);
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 1000; ++i) {
        futures.push_back(pool.submit([&counter]() {
            counter++;
        }));
    }

    for (auto &future : futures) {
        future.wait();
    }

    EXPECT_EQ(counter.load(), 1000);
}

TEST(ThreadPoolTest, ConcurrentSubmission) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    std::vector<std::thread> submitters;

    for (int t = 0; t < 8; ++t) {
        submitters.emplace_back([&pool, &counter]() {
            for (int i = 0; i < 100; ++i) {
                pool.submit([&counter]() {
                    counter++;
                });
            }
        });
    }

    for (auto &submitter : submitters) {
        submitter.join();
    }

    pool.waitForGlobalTasks();
    EXPECT_EQ(counter.load(), 800);
}

TEST(ThreadPoolTest, DynamicThreadAdjustmentUnderLoad) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};

    // Start submitting tasks
    std::thread submitter([&pool, &counter]() {
        for (int i = 0; i < 100; ++i) {
            pool.submit([&counter]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                counter++;
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Adjust thread count while tasks are running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW({
        pool.setThreadCount(4);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW({
        pool.setThreadCount(2);
    });

    submitter.join();
    pool.waitForGlobalTasks();
    EXPECT_EQ(counter.load(), 100);
}

// ==========================================
// === Complex Scenario Tests ===
// ==========================================

TEST(ThreadPoolTest, MixedPriorityTasks) {
    ThreadPool pool(4);
    std::atomic<int> highCounter{0};
    std::atomic<int> normalCounter{0};
    std::atomic<int> lowCounter{0};

    for (int i = 0; i < 30; ++i) {
        pool.submitWithPriority(neko::Priority::High, [&highCounter]() {
            highCounter++;
        });
        pool.submitWithPriority(neko::Priority::Normal, [&normalCounter]() {
            normalCounter++;
        });
        pool.submitWithPriority(neko::Priority::Low, [&lowCounter]() {
            lowCounter++;
        });
    }

    pool.waitForGlobalTasks();
    EXPECT_EQ(highCounter.load(), 30);
    EXPECT_EQ(normalCounter.load(), 30);
    EXPECT_EQ(lowCounter.load(), 30);
}

TEST(ThreadPoolTest, MixedGlobalAndPersonalTasks) {
    ThreadPool pool(4);
    auto workerIds = pool.getWorkerIds();
    std::atomic<int> globalCounter{0};
    std::atomic<int> personalCounter{0};

    std::vector<std::future<void>> personalFutures;
    for (int i = 0; i < 50; ++i) {
        pool.submit([&globalCounter]() {
            globalCounter++;
        });

        personalFutures.push_back(pool.submitToWorker(workerIds[i % workerIds.size()], [&personalCounter]() {
            personalCounter++;
        }));
    }

    pool.waitForGlobalTasks();
    
    // Wait for all personal tasks to complete
    for (auto &f : personalFutures) {
        f.wait();
    }

    EXPECT_EQ(globalCounter.load(), 50);
    EXPECT_EQ(personalCounter.load(), 50);
}

TEST(ThreadPoolTest, RecursiveTaskSubmission) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};

    std::function<void(int)> recursiveTask;
    recursiveTask = [&pool, &counter, &recursiveTask](int depth) {
        counter++;

        if (depth > 0) {

            pool.submit([&recursiveTask, depth]() {
                recursiveTask(depth - 1);
            });
        }
    };

    for (int i = 0; i < 10; ++i) {
        pool.submit([&recursiveTask]() {
            recursiveTask(3);
        });
    }

    pool.waitForGlobalTasks();
    EXPECT_EQ(counter.load(), 40);
}

// ==========================================
// === Edge Cases ===
// ==========================================

TEST(ThreadPoolTest, EmptyTaskQueue) {
    ThreadPool pool(2);
    EXPECT_EQ(pool.getPendingTaskCount(), 0u);
}

TEST(ThreadPoolTest, ImmediateStop) {
    ThreadPool pool(4);
    pool.stop();
    EXPECT_EQ(pool.getPendingTaskCount(), 0u);
}

TEST(ThreadPoolTest, MultipleStops) {
    ThreadPool pool(2);
    EXPECT_NO_THROW({
        pool.stop();
        pool.stop();
        pool.stop();
    });
}

TEST(ThreadPoolTest, LargeReturnValue) {
    ThreadPool pool(2);

    auto future = pool.submit([]() {
        return std::vector<int>(10000, 42);
    });

    auto result = future.get();
    EXPECT_EQ(result.size(), 10000u);
    EXPECT_EQ(result[0], 42);
}

// ==========================================
// === Performance Tests ===
// ==========================================

TEST(ThreadPoolTest, TaskThroughput) {
    ThreadPool pool(4);
    const int taskCount = 10000;
    std::atomic<int> counter{0};

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < taskCount; ++i) {
        pool.submit([&counter]() {
            counter++;
        });
    }

    pool.waitForGlobalTasks();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(counter.load(), taskCount);
    std::cout << "Processed " << taskCount << " tasks in " << duration.count() << " ms" << std::endl;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
