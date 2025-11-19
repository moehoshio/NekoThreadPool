/**
 * @brief vcpkg package test for NekoThreadPool
 * @file test_package.cpp
 */

#include <neko/thread/threadPool.hpp>
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    std::cout << "Testing NekoThreadPool installation via vcpkg..." << std::endl;

    try {
        // Create a thread pool with 4 threads
        neko::thread::ThreadPool pool(4);
        
        std::cout << "✓ ThreadPool created successfully" << std::endl;

        // Test basic task submission
        std::atomic<int> counter{0};
        
        for (int i = 0; i < 10; ++i) {
            pool.submit([&counter, i]() {
                counter++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            });
        }

        // Wait for tasks to complete
        pool.waitForGlobalTasks();
        
        if (counter == 10) {
            std::cout << "✓ All tasks completed successfully (counter = " << counter << ")" << std::endl;
        } else {
            std::cerr << "✗ Task execution failed (counter = " << counter << ", expected 10)" << std::endl;
            return 1;
        }

        // Test task with return value
        auto future = pool.submit([]() {
            return 42;
        });

        int result = future.get();
        if (result == 42) {
            std::cout << "✓ Task with return value works correctly" << std::endl;
        } else {
            std::cerr << "✗ Task return value incorrect (got " << result << ", expected 42)" << std::endl;
            return 1;
        }

        std::cout << "\n=== All vcpkg package tests passed! ===" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "✗ Exception caught: " << e.what() << std::endl;
        return 1;
    }
}
