cmake_minimum_required(VERSION 3.15)
project(VcpkgTestPackage CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the NekoThreadPool package installed via vcpkg
find_package(NekoThreadPool REQUIRED CONFIG)

# Create test executable
add_executable(vcpkg_test test_package.cpp)

# Link against NekoThreadPool
target_link_libraries(vcpkg_test PRIVATE Neko::ThreadPool)

# Enable testing
enable_testing()
add_test(NAME vcpkg_test COMMAND vcpkg_test)
