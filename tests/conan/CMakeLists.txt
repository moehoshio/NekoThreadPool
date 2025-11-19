cmake_minimum_required(VERSION 3.15)
project(ConanTestPackage CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the NekoThreadPool package installed via Conan
find_package(NekoThreadPool REQUIRED CONFIG)

# Create test executable
add_executable(conan_test test_package.cpp)

# Link against NekoThreadPool
target_link_libraries(conan_test PRIVATE Neko::ThreadPool)

# Enable testing
enable_testing()
add_test(NAME conan_test COMMAND conan_test)
