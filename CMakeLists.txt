cmake_minimum_required(VERSION 3.14)
project(NekoThreadPool VERSION 1.0 LANGUAGES CXX)

option(NEKO_AUTO_FETCH_DEPS "Automatically fetch dependencies" ON)
option(NEKO_BUILD_TESTS "Build tests" ON)

find_package(GTest QUIET)

# Print configuration summary
message(STATUS "NekoThreadPool configuration summary:")
message(STATUS "  - CMake version: ${CMAKE_VERSION}")
message(STATUS "")
message(STATUS "  - Neko Auto fetch deps: ${NEKO_AUTO_FETCH_DEPS}")
message(STATUS "  - Neko Build tests: ${NEKO_BUILD_TESTS}")
message(STATUS "")
message(STATUS "Dependency summary:")
message(STATUS "  - GoogleTest support: ${GTest_FOUND}")
message(STATUS "")


if(NEKO_AUTO_FETCH_DEPS)
    include(FetchContent)

    FetchContent_Declare(
        NekoSchema
        GIT_REPOSITORY https://github.com/moehoshio/NekoSchema.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(NekoSchema)

    if(NOT GTest_FOUND AND NEKO_BUILD_TESTS)
        message(STATUS "GTest not found. Fetching GoogleTest...")

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


add_library(NekoThreadPool INTERFACE)

target_include_directories(NekoThreadPool INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Dependencies
target_link_libraries(NekoThreadPool INTERFACE NekoSchema)
target_compile_features(NekoThreadPool INTERFACE cxx_std_20)

if(NEKO_BUILD_TESTS)
    enable_testing()
    message(STATUS "NekoThreadPool tests enabled")
    if (NOT NEKO_AUTO_FETCH_DEPS AND NOT GTest_FOUND)
        message(FATAL_ERROR "GTest is required for building tests. Please enable NEKO_AUTO_FETCH_DEPS or install GTest manually.")
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
