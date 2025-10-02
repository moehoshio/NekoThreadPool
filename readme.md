# NekoThreadPool

An easy-to-use and efficient C++ 20 thread pool that supports task priorities, statistics collection, and task submission to specific threads.

[![License](https://img.shields.io/badge/License-MIT%20OR%20Apache--2.0-blue.svg)](LICENSE)
![Require](https://img.shields.io/badge/%20Require%20-%3E=%20C++%2020-orange.svg)


## Features

- **Task Priority** - Support for tasks with different priorities
- **Personal Task Queue** - Support for assigning tasks to specific threads
- **Statistics Collection** - Detailed task execution statistics and execution time monitoring
- **Dynamic Management** - Support for adjusting thread count and maximum queue size at runtime

## Requirements

- C++20 or higher compatible compiler
- CMake 3.14 or higher (if using CMake)

## Quick Start

Configuration: [CMake](#cmake) | [Manual](#manual)

Example: [Basic Usage](#basic-usage) | [Tasks with Parameters](#tasks-with-parameters) | [](#set-maximum-queue-size) | [Dynamic Thread Count Adjustment](#dynamic-thread-count-adjustment) | [Custom Logging](#custom-logging) | [Statistics Control](#statistics-control) | [Error Handling](#error-handling)

Advanced: [Task Priority](#task-priority) | [Assign Tasks to Specific Threads](#assign-tasks-to-specific-threads) | [Waiting for Task Completion](#waiting-for-task-completion)

Statistics and Monitoring: [Get Statistics Information](#get-statistics-information) | [Real-time Utilization Monitoring](#real-time-utilization-monitoring)

API Reference: [Core Methods](#core-methods) | [Configuration Methods](#configuration-methods) | [Query Methods](#query-methods)

### CMake

1. Using CMake's `FetchContent` to include NekoThreadPool in your project:

```cmake
include(FetchContent)

# Add NekoThreadPool to your CMake project
FetchContent_Declare(
    NekoThreadPool
    GIT_REPOSITORY https://github.com/moehoshio/NekoThreadPool.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(NekoThreadPool)

target_link_libraries(your_target PRIVATE NekoThreadPool)
```

2. Include the header in your source code

```cpp
#include <neko/core/threadPool.hpp>
```

### Manual

When installing manually, you need to manually fetch the dependency [`NekoSchema`](https://github.com/moehoshio/NekoSchema).

After installing the dependency, please continue:

1. Clone or download the repository to your host

```sh
git clone https://github.com/moehoshio/NekoThreadPool.git
```

or

```sh
curl -L -o NekoThreadPool.zip https://github.com/moehoshio/NekoThreadPool/archive/refs/heads/main.zip

unzip NekoThreadPool.zip
```

2. Copy the contents of the `NekoThreadPool/include` folder into your project's `include` directory.

```shell
cp -r NekoThreadPool/include/ /path/to/your/include/
```

3. Include the header in your source code

```cpp
#include <neko/core/threadPool.hpp>
```

### Basic Usage

```cpp
#include <neko/core/threadPool.hpp>
#include <iostream>

int main() {
    // Create thread pool (uses hardware thread count by default)
    neko::core::thread::ThreadPool pool(2);
    
    // Submit simple task
    auto future = pool.submit([]() {
        return 42;
    });
    
    // Get result
    std::cout << "Result: " << future.get() << std::endl;
    
    return 0;
}
```

### Tasks with Parameters

```cpp
#include <neko/core/threadPool.hpp>
#include <iostream>

int add(int a, int b) {
    return a + b;
}

int main() {
    neko::core::thread::ThreadPool pool;
    
    // Submit function with parameters
    auto future1 = pool.submit(add, 10, 20);
    
    // Submit lambda expression
    auto future2 = pool.submit([](int x, int y) {
        return x * y;
    }, 5, 6);
    
    std::cout << "Addition result: " << future1.get() << std::endl;  // 30
    std::cout << "Multiplication result: " << future2.get() << std::endl;  // 30
    
    return 0;
}
```

## Advanced Features

### Task Priority

```cpp
#include <neko/core/threadPool.hpp>
#include "neko/schema/types.hpp"
#include <iostream>

int main() {
    neko::core::thread::ThreadPool pool;
    
    // High priority task
    auto highPriority = pool.submitWithPriority(
        neko::Priority::High,
        [](){}
    );
    
    // Low priority task
    auto lowPriority = pool.submitWithPriority(
        neko::Priority::Low,
        [](){}
    );
    
}
```

### Assign Tasks to Specific Threads

```cpp
#include <neko/core/threadPool.hpp>
#include <iostream>

int main() {
    neko::core::thread::ThreadPool pool(4);
    
    // Get available thread IDs
    auto workerIds = pool.getWorkerIds();
    
    if (!workerIds.empty()) {

        auto targetThreadId = workerIds[0];  // Assign to the first available thread

        pool.submitToWorker(targetThreadId, []() {
            std::cout << "Task executing on specific thread\n";
        });

        // Submit to each worker thread
        for (const auto & id : workerIds) {
            auto future = pool.submitToWorker(id, [id]() {
                std::cout << "Task executing on thread " << id << "\n";
            });
        }
    }
    
    return 0;
}
```

Note: Assigning to a non-existent thread ID will throw a `neko::ex::OutOfRange` exception.

### Waiting for Task Completion

```cpp
#include <neko/core/threadPool.hpp>
#include <chrono>
#include <iostream>

int main() {
    neko::core::thread::ThreadPool pool;
    
    // Submit multiple tasks
    for (int i = 0; i < 10; ++i) {
        pool.submit([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::cout << "Task " << i << " completed\n";
        });
    }
    
    // Wait for all tasks to complete
    pool.waitForAllTasksCompletion();
    std::cout << "All tasks completed\n";
    
    // Or use timeout waiting
    bool completed = pool.waitForAllTasksCompletion(std::chrono::seconds(5));
    if (completed) {
        std::cout << "Tasks completed within 5 seconds\n";
    } else {
        std::cout << "Tasks not completed within 5 seconds\n";
    }
    
    return 0;
}
```

## Statistics and Monitoring

### Get Statistics Information

```cpp
#include <neko/core/threadPool.hpp>
#include <iostream>

int main() {
    neko::core::thread::ThreadPool pool;
    
    // Submit some tasks
    for (int i = 0; i < 100; ++i) {
        pool.submit([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return i * i;
        });
    }
    
    // Wait for completion
    pool.waitForAllTasksCompletion();
    
    // Get statistics information
    auto stats = pool.getTaskStats();
    
    std::cout << "Completed tasks: " << stats.completedTasks << std::endl;
    std::cout << "Failed tasks: " << stats.failedTasks << std::endl;
    std::cout << "Total execution time: " << stats.totalExecutionTime.count() << "ms\n";
    std::cout << "Max execution time: " << stats.maxExecutionTime.count() << "ms\n";
    std::cout << "Average execution time: " << stats.avgExecutionTime.count() << "ms\n";
    
    return 0;
}
```

### Real-time Utilization Monitoring

```cpp
#include <neko/core/threadPool.hpp>
#include <iostream>
#include <thread>

int main() {
    neko::core::thread::ThreadPool pool(4);
    
    // Submit long-running tasks
    for (int i = 0; i < 10; ++i) {
        pool.submit([]() {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        });
    }
    
    // Monitor thread pool status
    for (int i = 0; i < 5; ++i) {
        std::cout << "Queue utilization: " << pool.getQueueUtilization() * 100 << "%\n";
        std::cout << "Thread utilization: " << pool.getThreadUtilization() * 100 << "%\n";
        std::cout << "Pending tasks: " << pool.getPendingTaskCount() << std::endl;
        std::cout << "---\n";
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    return 0;
}
```

## Configuration Options

### Set Maximum Queue Size

```cpp
neko::core::thread::ThreadPool pool;

// Set maximum queue size to 1000
pool.setMaxQueueSize(1000);

// Check if queue is full
if (pool.isQueueFull()) {
    std::cout << "Queue is full, cannot submit more tasks\n";
}
```

### Dynamic Thread Count Adjustment

```cpp
neko::core::thread::ThreadPool pool(2);

std::cout << "Initial thread count: " << pool.getThreadCount() << std::endl;

// Increase threads
pool.setThreadCount(8);
std::cout << "Adjusted thread count: " << pool.getThreadCount() << std::endl;

// Decrease threads
pool.setThreadCount(4);
std::cout << "Final thread count: " << pool.getThreadCount() << std::endl;
```

### Custom Logging

```cpp
neko::core::thread::ThreadPool pool;

// Set logger function
pool.setLogger([](const std::string& message) {
    std::cout << "[ThreadPool] " << message << std::endl;
});

// Now the thread pool will log important events
```

### Statistics Control

```cpp
neko::core::thread::ThreadPool pool;

// Disable statistics (improve performance)
pool.enableStatistics(false);

// Re-enable statistics
pool.enableStatistics(true);

// Reset statistics data
pool.resetStats();
```

## Error Handling

```cpp
#include <neko/core/threadPool.hpp>
#include <neko/schema/exception.hpp>
#include <iostream>

int main() {
    try {
        neko::core::thread::ThreadPool pool;
        
        // Set small queue size
        pool.setMaxQueueSize(2);
        
        // Try to submit more tasks than queue size
        for (int i = 0; i < 5; ++i) {
            try {
                pool.submit([]() {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                });
                std::cout << "Task " << i << " submitted\n";
            } catch (const ex::TaskRejected& e) {
                std::cout << "Task " << i << " rejected: " << e.what() << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}
```

## API Reference

### Core Methods

| Method | Description |
|--------|-------------|
| `submit(function, args...)` | Submit normal priority task |
| `submitWithPriority(priority, function, args...)` | Submit task with specified priority |
| `submitToWorker(workerId, function, args...)` | Submit task to specific thread |
| `waitForTasksEmpty()` | Wait for queue to be empty |
| `waitForAllTasksCompletion()` | Wait for all tasks to complete |
| `stop(waitForCompletion)` | Stop thread pool |

### Configuration Methods

| Method | Description |
|--------|-------------|
| `setThreadCount(count)` | Set thread count |
| `setMaxQueueSize(size)` | Set maximum queue size |
| `enableStatistics(enable)` | Enable/disable statistics |
| `setLogger(loggerFunc)` | Set logger function |
| `resetStats()` | Reset statistics data |

### Query Methods

| Method | Description |
|--------|-------------|
| `getThreadCount()` | Get thread count |
| `getPendingTaskCount()` | Get pending task count |
| `getTaskStats()` | Get task statistics |
| `getQueueUtilization()` | Get queue utilization |
| `getThreadUtilization()` | Get thread utilization |
| `isEmpty()` | Check if no tasks |
| `isQueueFull()` | Check if queue is full |

## License

[License](LICENSE) MIT OR Apache-2.0
