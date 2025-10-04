cmake_minimum_required(VERSION 3.14)
project(NekoThreadPool VERSION 1.0 LANGUAGES CXX)

option(NEKO_AUTO_FETCH_DEPS "Automatically fetch dependencies" ON)
option(NEKO_BUILD_TESTS "Build tests" ON)

if(NEKO_AUTO_FETCH_DEPS)
    include(FetchContent)

    FetchContent_Declare(
        NekoSchema
        GIT_REPOSITORY https://github.com/moehoshio/NekoSchema.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(NekoSchema)

    FetchContent_Declare(
        nlog
        GIT_REPOSITORY https://github.com/moehoshio/nlog.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(nlog)

    if(NEKO_BUILD_TESTS)
        find_package(GTest QUIET)

        if(NOT GTest_FOUND)
        # Fetch GoogleTest
            include(FetchContent)
            FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG        release-1.12.1
            )
            # For Windows: Prevent overriding the parent project's compiler/linker settings
            set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
            FetchContent_MakeAvailable(googletest)
        endif(NOT GTest_FOUND)

    endif(NEKO_BUILD_TESTS)

endif(NEKO_AUTO_FETCH_DEPS)


add_library(NekoThreadPool INTERFACE)

target_include_directories(NekoThreadPool INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Dependencies
target_link_libraries(NekoThreadPool INTERFACE NekoSchema Neko::Log)
target_compile_features(NekoThreadPool INTERFACE cxx_std_20)

if(NEKO_BUILD_TESTS)
    if (NOT NEKO_AUTO_FETCH_DEPS AND NOT GTest_FOUND)
        message(FATAL_ERROR "GTest is required for building tests. Please enable NEKO_AUTO_FETCH_DEPS or install GTest manually.")
    endif()

    enable_testing()
    add_executable(threadPool_test tests/threadPool_test.cpp)
    target_include_directories(threadPool_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(threadPool_test PRIVATE NekoThreadPool gtest_main)
    target_compile_features(threadPool_test PRIVATE cxx_std_20)

    include(GoogleTest)
    gtest_discover_tests(threadPool_test)
    
endif(NEKO_BUILD_TESTS)
