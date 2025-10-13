cmake_minimum_required(VERSION 3.14)
project(NekoThreadPool VERSION 1.0 LANGUAGES CXX)

# ================
# === Config ====
# ================

option(NEKO_THREAD_POOL_BUILD_TESTS "Neko Thread Pool Build tests" ON)
option(NEKO_THREAD_POOL_AUTO_FETCH_DEPS "Neko Thread Pool Automatically fetch dependencies" ON)


# Print configuration summary
message(STATUS "Start configuration Neko Thread Pool...")
message(STATUS "NekoThreadPool configuration summary:")
message(STATUS "  - CMake version: ${CMAKE_VERSION}")
message(STATUS "")
message(STATUS "  - Neko Thread Pool Auto fetch deps: ${NEKO_THREAD_POOL_AUTO_FETCH_DEPS}")
message(STATUS "  - Neko Thread Pool Build tests: ${NEKO_THREAD_POOL_BUILD_TESTS}")
message(STATUS "")
message(STATUS "Dependency summary:")
message(STATUS "  - No dependencies required at configuration time")
message(STATUS "")

if(NEKO_THREAD_POOL_AUTO_FETCH_DEPS)
    include(FetchContent)

    FetchContent_Declare(
        NekoSchema
        GIT_REPOSITORY https://github.com/moehoshio/NekoSchema.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(NekoSchema)

    if(NEKO_THREAD_POOL_BUILD_TESTS)
        message(STATUS "Neko Thread Pool Fetching GoogleTest...")

        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        release-1.12.1
        )
        # For Windows: Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

endif(NEKO_AUTO_FETCH_DEPS)


# ================
# = Main target =
# ================

add_library(NekoThreadPool INTERFACE)
add_library(Neko::ThreadPool ALIAS NekoThreadPool)

target_include_directories(NekoThreadPool INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Dependencies
target_link_libraries(NekoThreadPool INTERFACE NekoSchema)
target_compile_features(NekoThreadPool INTERFACE cxx_std_20)

# ================
# ==== Tests =====
# ================

if(NEKO_THREAD_POOL_BUILD_TESTS)
    enable_testing()
    message(STATUS "NekoThreadPool tests enabled")

    if (NOT NEKO_THREAD_POOL_AUTO_FETCH_DEPS)
        message(FATAL_ERROR "GTest is required for building tests. Please enable NEKO_THREAD_POOL_AUTO_FETCH_DEPS.")
    endif()

    add_executable(threadPool_test tests/threadPool_test.cpp)
    target_include_directories(threadPool_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(threadPool_test PRIVATE NekoThreadPool gtest_main)
    target_compile_features(threadPool_test PRIVATE cxx_std_20)

    include(GoogleTest)
    gtest_discover_tests(threadPool_test)
    
else()
    message(STATUS "NekoThreadPool tests disabled (NEKO_BUILD_TESTS=OFF)")
endif()

message(STATUS "End Neko Thread Pool")