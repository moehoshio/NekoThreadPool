# NekoThreadPool

An easy-to-use and efficient C++ 20 thread pool that supports task priorities, statistics collection, and task submission to specific threads.

[![License](https://img.shields.io/badge/License-MIT%20OR%20Apache--2.0-blue.svg)](LICENSE)
![Require](https://img.shields.io/badge/%20Require%20-%3E=%20C++%2020-orange.svg)
[![CMake](https://img.shields.io/badge/CMake-3.14+-green.svg)](https://cmake.org/)

## Features

- **Task Priority** - Support for tasks with different priorities
- **Personal Task Queue** - Support for assigning tasks to specific threads
- **Dynamic Management** - Support for adjusting thread count and maximum queue size at runtime

## Requirements

- C++20 or higher compatible compiler
- CMake 3.14 or higher (if using CMake)

## Quick Start

Configuration: [CMake](#cmake) | [Manual](#manual)

Example: [Basic Usage](#basic-usage) | [Tasks with Parameters](#tasks-with-parameters) | [](#set-maximum-queue-size) | [Dynamic Thread Count Adjustment](#dynamic-thread-count-adjustment) | [Error Handling](#error-handling)

Advanced: [Task Priority](#task-priority) | [Assign Tasks to Specific Threads](#assign-tasks-to-specific-threads) | [Waiting for Task Completion](#waiting-for-task-completion)

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

# Add your target and link NekoLog
add_executable(your_target main.cpp)

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

## License

[License](LICENSE) MIT OR Apache-2.0

## See More

- [NekoLog](https://github.com/moehoshio/nlog): An easy-to-use, modern, lightweight, and efficient C++20 logging library.
- [NekoEvent](https://github.com/moehoshio/NekoEvent): A modern easy to use type-safe and high-performance event handling system for C++.
- [NekoSchema](https://github.com/moehoshio/NekoSchema): A lightweight, header-only C++20 schema library.
- [NekoSystem](https://github.com/moehoshio/NekoSystem): A modern C++20 cross-platform system utility library.
- [NekoFunction](https://github.com/moehoshio/NekoFunction): A comprehensive modern C++ utility library that provides practical functions for common programming tasks.
- [NekoThreadPool](https://github.com/moehoshio/NekoThreadPool): An easy to use and efficient C++ 20 thread pool that supports priorities and submission to specific threads.