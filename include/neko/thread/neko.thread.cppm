/**
 * @file neko.thread.cppm
 * @brief C++20 module interface for NekoThread
 * @details This module exports all NekoThread functionality by wrapping the header files.
 *          The original headers are still available for traditional include-based usage.
 */

module;

#if !defined(__cpp_lib_modules) || (__cpp_lib_modules < 202207L)
// Global module fragment - include headers that should not be exported
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
#include <string>
#endif

export module neko.thread;

import neko.schema;

#define NEKO_THREAD_POOL_ENABLE_MODULE true

export {
    #include "threadPool.hpp"
}
