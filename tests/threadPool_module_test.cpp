/**
 * @brief Neko Thread Pool Module Tests
 * @file threadPool_module_test.cpp
 * @author moehoshio
 * @copyright Copyright (c) 2025 Hoshi
 * @license MIT OR Apache-2.0
 * @details Tests using C++20 module import instead of traditional headers
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <numeric>
#include <thread>
#include <vector>
#include <unordered_set>

import neko.thread;

using namespace neko::thread;

// ==========================================
// === Module Import Verification Tests ===
// ==========================================

TEST(ModuleTest, VerifyModuleImport) {
    // Verify that we can create a ThreadPool using module import
    EXPECT_NO_THROW({
        ThreadPool pool(2);
    });
}

TEST(ModuleTest, VerifyPriorityEnum) {
    // Verify that Priority enum is accessible from module
    EXPECT_NO_THROW({
        neko::Priority high = neko::Priority::High;
        neko::Priority normal = neko::Priority::Normal;
        neko::Priority low = neko::Priority::Low;
        
        EXPECT_NE(high, normal);
        EXPECT_NE(normal, low);
        EXPECT_NE(high, low);
    });
}

// ==========================================
// === Basic Module Functionality Tests ===
// ==========================================

TEST(ModuleTest, BasicConstruction) {
    EXPECT_NO_THROW({
        ThreadPool pool(4);
        EXPECT_EQ(pool.getThreadCount(), 4u);
    });
}

TEST(ModuleTest, SubmitSimpleTask) {
    ThreadPool pool(2);
    std::atomic<bool> taskExecuted{false};

    auto future = pool.submit([&taskExecuted]() {
        taskExecuted = true;
    });

    future.wait();
    EXPECT_TRUE(taskExecuted.load());
}

TEST(ModuleTest, SubmitTaskWithReturnValue) {
    ThreadPool pool(2);

    auto future = pool.submit([]() {
        return 42;
    });

    EXPECT_EQ(future.get(), 42);
}

TEST(ModuleTest, SubmitTaskWithParameters) {
    ThreadPool pool(2);

    auto add = [](int a, int b) {
        return a + b;
    };

    auto future = pool.submit(add, 10, 20);
    EXPECT_EQ(future.get(), 30);
}

// ==========================================
// === Module Priority Tests ===
// ==========================================

TEST(ModuleTest, SubmitWithPriority) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};

    auto highFuture = pool.submitWithPriority(neko::Priority::High, [&counter]() {
        counter++;
        return 1;
    });

    auto normalFuture = pool.submitWithPriority(neko::Priority::Normal, [&counter]() {
        counter++;
        return 2;
    });

    auto lowFuture = pool.submitWithPriority(neko::Priority::Low, [&counter]() {
        counter++;
        return 3;
    });

    highFuture.wait();
    normalFuture.wait();
    lowFuture.wait();

    EXPECT_EQ(counter.load(), 3);
}

// ==========================================
// === Module Personal Queue Tests ===
// ==========================================

TEST(ModuleTest, SubmitToWorker) {
    ThreadPool pool(4);
    auto workerIds = pool.getWorkerIds();
    ASSERT_FALSE(workerIds.empty());

    std::atomic<int> counter{0};
    auto future = pool.submitToWorker(workerIds[0], [&counter]() {
        counter++;
        return counter.load();
    });

    int result = future.get();
    EXPECT_EQ(result, 1);
    EXPECT_EQ(counter.load(), 1);
}

TEST(ModuleTest, SubmitToInvalidWorker) {
    ThreadPool pool(2);
    
    EXPECT_THROW({
        pool.submitToWorker(9999, []() {});
    }, neko::ex::OutOfRange);
}

// ==========================================
// === Module Statistics Tests ===
// ==========================================

TEST(ModuleTest, GetWorkerIds) {
    ThreadPool pool(4);
    auto workerIds = pool.getWorkerIds();
    
    EXPECT_EQ(workerIds.size(), 4u);
    
    // Verify all IDs are unique
    std::unordered_set<neko::uint64> uniqueIds(workerIds.begin(), workerIds.end());
    EXPECT_EQ(uniqueIds.size(), workerIds.size());
}

TEST(ModuleTest, GetThreadCount) {
    ThreadPool pool(6);
    EXPECT_EQ(pool.getThreadCount(), 6u);
}

TEST(ModuleTest, GetThreadUtilization) {
    ThreadPool pool(4);
    auto utilization = pool.getThreadUtilization();
    
    EXPECT_GE(utilization, 0.0);
    EXPECT_LE(utilization, 1.0);
}

TEST(ModuleTest, GetQueueUtilization) {
    ThreadPool pool(2);
    auto utilization = pool.getQueueUtilization();
    
    EXPECT_GE(utilization, 0.0);
    EXPECT_LE(utilization, 1.0);
}

TEST(ModuleTest, GetPendingTaskCount) {
    ThreadPool pool(1);
    
    std::mutex mtx;
    std::condition_variable cv;
    bool blockingStarted = false;

    // Block the worker with a task
    auto blockingFuture = pool.submit([&]() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            blockingStarted = true;
        }
        cv.notify_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    });

    // Wait for blocking task to start
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(1), [&] { return blockingStarted; });
    }

    // Add pending tasks
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool.submit([]() {}));
    }

    EXPECT_GT(pool.getPendingTaskCount(), 0u);

    // Cleanup
    blockingFuture.wait();
    for (auto &f : futures) {
        f.wait();
    }
}

// ==========================================
// === Module Queue Management Tests ===
// ==========================================

TEST(ModuleTest, GetMaxQueueSize) {
    ThreadPool pool(2);
    EXPECT_GT(pool.getMaxQueueSize(), 0u);
}

TEST(ModuleTest, SetMaxQueueSize) {
    ThreadPool pool(2);
    pool.setMaxQueueSize(100);
    EXPECT_EQ(pool.getMaxQueueSize(), 100u);
}

TEST(ModuleTest, IsQueueFull) {
    ThreadPool pool(1);
    pool.setMaxQueueSize(3);

    std::mutex mtx;
    std::condition_variable cv;
    bool blockingStarted = false;

    // Block the worker
    auto blockingFuture = pool.submit([&]() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            blockingStarted = true;
        }
        cv.notify_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    });

    // Wait for blocking task to start
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(1), [&] { return blockingStarted; });
    }

    EXPECT_FALSE(pool.isQueueFull());

    // Fill the queue
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 3; ++i) {
        futures.push_back(pool.submit([]() {}));
    }

    EXPECT_TRUE(pool.isQueueFull());

    // Cleanup
    blockingFuture.wait();
    for (auto &f : futures) {
        f.wait();
    }
}

// ==========================================
// === Module Thread Management Tests ===
// ==========================================

TEST(ModuleTest, SetThreadCountIncrease) {
    ThreadPool pool(2);
    EXPECT_EQ(pool.getThreadCount(), 2u);

    pool.setThreadCount(4);
    EXPECT_EQ(pool.getThreadCount(), 4u);
    
    // Verify new threads can execute tasks
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 8; ++i) {
        futures.push_back(pool.submit([&counter]() {
            counter++;
        }));
    }
    
    for (auto &f : futures) {
        f.wait();
    }
    
    EXPECT_EQ(counter.load(), 8);
}

TEST(ModuleTest, SetThreadCountDecrease) {
    ThreadPool pool(4);
    EXPECT_EQ(pool.getThreadCount(), 4u);

    pool.setThreadCount(2);
    EXPECT_EQ(pool.getThreadCount(), 2u);
    
    // Verify remaining threads still work
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 4; ++i) {
        futures.push_back(pool.submit([&counter]() {
            counter++;
        }));
    }
    
    for (auto &f : futures) {
        f.wait();
    }
    
    EXPECT_EQ(counter.load(), 4);
}

TEST(ModuleTest, SetThreadCountZero) {
    ThreadPool pool(4);
    pool.setThreadCount(0);
    EXPECT_GE(pool.getThreadCount(), 1u);
}

// ==========================================
// === Module Wait Tests ===
// ==========================================

TEST(ModuleTest, WaitForGlobalTasks) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};

    for (int i = 0; i < 20; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            counter++;
        });
    }

    pool.waitForGlobalTasks();
    EXPECT_EQ(counter.load(), 20);
}

TEST(ModuleTest, WaitForGlobalTasksWithTimeout) {
    ThreadPool pool(2);

    // Submit quick tasks
    for (int i = 0; i < 10; ++i) {
        pool.submit([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        });
    }

    bool completed = pool.waitForGlobalTasks(std::chrono::seconds(5));
    EXPECT_TRUE(completed);
}

TEST(ModuleTest, WaitForGlobalTasksTimeout) {
    ThreadPool pool(1);
    std::atomic<bool> shouldContinue{true};

    // Submit a long task that can be interrupted
    auto future = pool.submit([&shouldContinue]() {
        for (int i = 0; i < 100 && shouldContinue.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    bool completed = pool.waitForGlobalTasks(std::chrono::milliseconds(100));
    EXPECT_FALSE(completed);

    // Signal the task to finish and stop the pool
    shouldContinue = false;
    pool.stop(true);
}

// ==========================================
// === Module Stop Tests ===
// ==========================================

TEST(ModuleTest, StopWithWait) {
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

TEST(ModuleTest, StopWithoutWait) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            counter++;
        });
    }

    pool.stop(false);
    EXPECT_LE(counter.load(), 100);
}

TEST(ModuleTest, SubmitAfterStop) {
    ThreadPool pool(2);
    pool.stop();

    EXPECT_THROW({
        pool.submit([]() {});
    }, neko::ex::ProgramExit);
}

// ==========================================
// === Module Exception Handling Tests ===
// ==========================================

TEST(ModuleTest, TaskWithException) {
    ThreadPool pool(2);

    auto future = pool.submit([]() {
        throw std::runtime_error("Module test exception");
    });

    EXPECT_THROW({
        future.get();
    }, std::runtime_error);
}

TEST(ModuleTest, MultipleTasksWithExceptions) {
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
// === Module Stress Tests ===
// ==========================================

TEST(ModuleTest, HighLoadSubmission) {
    ThreadPool pool(8);
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;

    const int taskCount = 1000;
    for (int i = 0; i < taskCount; ++i) {
        futures.push_back(pool.submit([&counter]() {
            counter++;
        }));
    }

    for (auto &future : futures) {
        future.wait();
    }

    EXPECT_EQ(counter.load(), taskCount);
}

TEST(ModuleTest, ConcurrentSubmission) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    std::vector<std::thread> submitters;

    const int numSubmitters = 8;
    const int tasksPerSubmitter = 100;

    for (int t = 0; t < numSubmitters; ++t) {
        submitters.emplace_back([&pool, &counter]() {
            for (int i = 0; i < tasksPerSubmitter; ++i) {
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
    EXPECT_EQ(counter.load(), numSubmitters * tasksPerSubmitter);
}

// ==========================================
// === Module Complex Scenario Tests ===
// ==========================================

TEST(ModuleTest, MixedPriorityTasks) {
    ThreadPool pool(4);
    std::atomic<int> highCounter{0};
    std::atomic<int> normalCounter{0};
    std::atomic<int> lowCounter{0};

    const int tasksPerPriority = 20;

    for (int i = 0; i < tasksPerPriority; ++i) {
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
    EXPECT_EQ(highCounter.load(), tasksPerPriority);
    EXPECT_EQ(normalCounter.load(), tasksPerPriority);
    EXPECT_EQ(lowCounter.load(), tasksPerPriority);
}

TEST(ModuleTest, MixedGlobalAndPersonalTasks) {
    ThreadPool pool(4);
    auto workerIds = pool.getWorkerIds();
    std::atomic<int> globalCounter{0};
    std::atomic<int> personalCounter{0};

    std::vector<std::future<void>> personalFutures;
    
    const int taskCount = 40;

    for (int i = 0; i < taskCount; ++i) {
        pool.submit([&globalCounter]() {
            globalCounter++;
        });

        personalFutures.push_back(pool.submitToWorker(workerIds[i % workerIds.size()], [&personalCounter]() {
            personalCounter++;
        }));
    }

    pool.waitForGlobalTasks();
    
    for (auto &f : personalFutures) {
        f.wait();
    }

    EXPECT_EQ(globalCounter.load(), taskCount);
    EXPECT_EQ(personalCounter.load(), taskCount);
}

TEST(ModuleTest, RecursiveTaskSubmission) {
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

    std::vector<std::future<void>> futures;
    const int initialCalls = 10;
    const int recursionDepth = 3;

    for (int i = 0; i < initialCalls; ++i) {
        futures.push_back(pool.submit([&recursiveTask, recursionDepth]() {
            recursiveTask(recursionDepth);
        }));
    }

    for (auto &f : futures) {
        f.wait();
    }

    pool.waitForGlobalTasks();
    
    // Each call with depth 3 creates: 1 + 1 + 1 + 1 = 4 tasks
    EXPECT_EQ(counter.load(), initialCalls * (recursionDepth + 1));
}

// ==========================================
// === Module Performance Tests ===
// ==========================================

TEST(ModuleTest, TaskThroughput) {
    ThreadPool pool(4);
    const int taskCount = 5000;
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
    std::cout << "[Module] Processed " << taskCount << " tasks in " << duration.count() << " ms" << std::endl;
}

TEST(ModuleTest, DynamicThreadAdjustmentUnderLoad) {
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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
