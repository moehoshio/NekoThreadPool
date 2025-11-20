# NekoThreadPool

An easy-to-use and efficient C++ 20 thread pool that supports task priorities and task submission to specific threads.

[![License](https://img.shields.io/badge/License-MIT%20OR%20Apache--2.0-blue.svg)](LICENSE)
![Require](https://img.shields.io/badge/%20Require%20-%3E=%20C++%2020-orange.svg)
[![CMake](https://img.shields.io/badge/CMake-3.14+-green.svg)](https://cmake.org/)
![Module Support](https://img.shields.io/badge/Modules-C%2B%2B20-blueviolet.svg)

## Features

- **Task Priority** - Support for tasks with different priorities
- **Personal Task Queue** - Support for assigning tasks to specific threads
- **Dynamic Management** - Support for adjusting thread count and maximum queue size at runtime
- **C++20 Module Support** - Optional support for C++20 modules for modern C++ projects

## Requirements

- C++20 or higher compatible compiler
- CMake 3.14 or higher (if using CMake)
- Git

## Quick Start

Configuration: [CMake](#cmake) | [vcpkg](#vcpkg) | [Conan](#conan) |[Manual](#manual) | [Test](#test)

Example: [Basic Usage](#basic-usage) | [Tasks with Parameters](#tasks-with-parameters) | [Set Maximum Queue Size](#set-maximum-queue-size) | [Dynamic Thread Count Adjustment](#dynamic-thread-count-adjustment) | [Error Handling](#error-handling)

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

# Add your target and link NekoThreadPool
add_executable(your_target main.cpp)

target_link_libraries(your_target PRIVATE Neko::ThreadPool)
```

2. Include the header in your source code

```cpp
#include <neko/thread/threadPool.hpp>
```

#### CMake with Module Support

To enable C++20 module support, use the `NEKO_THREAD_POOL_ENABLE_MODULE` option:

```cmake
FetchContent_Declare(
    ...
)

# Set Options Before Building
set(NEKO_THREAD_POOL_ENABLE_MODULE ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(NekoThreadPool)

...

target_link_libraries(your_target PRIVATE Neko::ThreadPool::Module)
```

Import the module in your source code:

```cpp
import neko.thread;
```

### vcpkg

Install NekoThreadPool using vcpkg:

```shell
vcpkg install neko-threadpool
```

Or add it to your `vcpkg.json`:

```json
{
  "dependencies": ["neko-threadpool"]
}
```

Then in your CMakeLists.txt:

```cmake
find_package(NekoThreadPool CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE Neko::ThreadPool)
```

When configuring your project, specify the vcpkg toolchain file:

```shell
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/vcpkg/installed/x64-windows
cmake --build build --config Debug
```

Note: Installing via vcpkg does not support modules.

### Conan

Add NekoThreadPool to your `conanfile.txt`:

```ini
[requires]
neko-threadpool/*

[generators]
CMakeDeps
CMakeToolchain
```

Or use it in your `conanfile.py`:

```python
from conan import ConanFile

class YourProject(ConanFile):
    requires = "neko-threadpool/*"
    generators = "CMakeDeps", "CMakeToolchain"
```

Then install and use:

```shell
conan install . --build=missing
cmake -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake
cmake --build build
```

In your CMakeLists.txt:

```cmake
find_package(NekoThreadPool CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE Neko::ThreadPool)
```

#### Conan with C++20 Module Support

To enable C++20 module support with Conan, use the `enable_module` option:

```shell
conan install . --build=missing -o neko-threadpool/*:enable_module=True
```

Or specify it in your `conanfile.txt`:

```ini
[requires]
neko-threadpool/*

[options]
neko-threadpool/*:enable_module=True

[generators]
CMakeDeps
CMakeToolchain
```

Or in your `conanfile.py`:

```python
from conan import ConanFile

class YourProject(ConanFile):
    requires = "neko-threadpool/*"
    generators = "CMakeDeps", "CMakeToolchain"
    
    def configure(self):
        self.options["neko-threadpool"].enable_module = True
```

Then link against the module target in your CMakeLists.txt:

```cmake
find_package(NekoThreadPool CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE Neko::ThreadPool::Module)
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
#include <neko/thread/threadPool.hpp>
#include <iostream>

int main() {
    // Create thread pool (uses hardware thread count by default)
    neko::thread::ThreadPool pool(2);
    
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
#include <neko/thread/threadPool.hpp>
#include <iostream>

int add(int a, int b) {
    return a + b;
}

int main() {
    neko::thread::ThreadPool pool;
    
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
#include <neko/thread/threadPool.hpp>
#include <neko/schema/types.hpp>
#include <iostream>

int main() {
    neko::thread::ThreadPool pool;
    
    // High priority task
    auto highPriority = pool.submitWithPriority(
        neko::Priority::High,
        [](){
            std::cout << "High priority task executed\n";
        }
    );
    
    // Low priority task
    auto lowPriority = pool.submitWithPriority(
        neko::Priority::Low,
        [](){
            std::cout << "Low priority task executed\n";
        }
    );
    
    // Wait for completion
    highPriority.wait();
    lowPriority.wait();
    
    return 0;
}
```

### Assign Tasks to Specific Threads

```cpp
#include <neko/thread/threadPool.hpp>
#include <iostream>

int main() {
    neko::thread::ThreadPool pool(4);
    
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
#include <neko/thread/threadPool.hpp>
#include <chrono>
#include <iostream>

int main() {
    neko::thread::ThreadPool pool;
    
    // Submit multiple tasks
    for (int i = 0; i < 10; ++i) {
        pool.submit([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::cout << "Task " << i << " completed\n";
        });
    }
    
    // Wait for all global tasks to complete
    pool.waitForGlobalTasks();
    std::cout << "All tasks completed\n";
    
    // Or use timeout waiting
    bool completed = pool.waitForGlobalTasks(std::chrono::seconds(5));
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
neko::thread::ThreadPool pool;

// Set maximum queue size to 1000
pool.setMaxQueueSize(1000);

// Check if queue is full
if (pool.isQueueFull()) {
    std::cout << "Queue is full, cannot submit more tasks\n";
}
```

### Dynamic Thread Count Adjustment

```cpp
neko::thread::ThreadPool pool(2);

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
#include <neko/thread/threadPool.hpp>
#include <neko/schema/exception.hpp>
#include <iostream>

int main() {
    try {
        neko::thread::ThreadPool pool;
        
        // Set small queue size
        pool.setMaxQueueSize(2);
        
        // Try to submit more tasks than queue size
        for (int i = 0; i < 5; ++i) {
            try {
                pool.submit([]() {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                });
                std::cout << "Task " << i << " submitted\n";
            } catch (const neko::ex::TaskRejected& e) {
                std::cout << "Task " << i << " rejected: " << e.what() << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}
```

## Test

You can run the tests to verify that everything is working correctly.

If you haven't configured the build yet, please run:

```shell
cmake -D NEKO_THREAD_POOL_BUILD_TESTS=ON -D NEKO_THREAD_POOL_AUTO_FETCH_DEPS=ON -B ./build -S .
```

Now, you can build the test files with the following command:

```shell
cmake --build ./build --config Debug
```

Then, you can run the tests with the following commands:

```shell
cd ./build && ctest -C Debug --output-on-failure
```

If everything is set up correctly, you should see output similar to the following:

```shell


  Test project /path/to/NekoThreadPool/build
      Start 1: NekoThreadPool_tests
  1/1 Test #1: NekoThreadPool_tests ...............   Passed    0.21 sec

  100% tests passed, 0 tests failed out of 1

  Total Test time (real) =   0.21 sec
```

### Disable Tests

If you want to disable building and running tests, you can set the following CMake option when configuring your project:

```shell
cmake -B ./build -DNEKO_THREAD_POOL_BUILD_TESTS=OFF -S .
```

This will skip test targets during the build process.

## License

[License](LICENSE) MIT OR Apache-2.0

## See More

- [NekoNet](https://github.com/moehoshio/NekoNet): A modern , easy-to-use C++20 networking library via libcurl.
- [NekoLog](https://github.com/moehoshio/NekoLog): An easy-to-use, modern, lightweight, and efficient C++20 logging library.
- [NekoEvent](https://github.com/moehoshio/NekoEvent): A modern easy to use type-safe and high-performance event handling system for C++.
- [NekoSchema](https://github.com/moehoshio/NekoSchema): A lightweight, header-only C++20 schema library.
- [NekoSystem](https://github.com/moehoshio/NekoSystem): A modern C++20 cross-platform system utility library.
- [NekoFunction](https://github.com/moehoshio/NekoFunction): A comprehensive modern C++ utility library that provides practical functions for common programming tasks.
- [NekoThreadPool](https://github.com/moehoshio/NekoThreadPool): An easy to use and efficient C++ 20 thread pool that supports priorities and submission to specific threads.
