cmake_minimum_required(VERSION 3.14)
project(NekoThreadPool VERSION 1.0.2 LANGUAGES CXX)

# ================
# === Config ====
# ================

option(NEKO_THREAD_POOL_BUILD_TESTS "Neko Thread Pool Build tests" ON)
option(NEKO_THREAD_POOL_AUTO_FETCH_DEPS "Neko Thread Pool Automatically fetch dependencies" ON)
option(NEKO_THREAD_POOL_ENABLE_MODULE "Neko Thread Pool Enable C++20 module" OFF)

find_package(NekoSchema QUIET)
find_package(GTest QUIET)

# Print configuration summary
message(STATUS "Start configuration Neko Thread Pool...")
message(STATUS "")
message(STATUS "NekoThreadPool configuration summary:")
message(STATUS "  - CMake version: ${CMAKE_VERSION}")
message(STATUS "")
message(STATUS "  - Neko Thread Pool Auto fetch deps: ${NEKO_THREAD_POOL_AUTO_FETCH_DEPS}")
message(STATUS "  - Neko Thread Pool Build tests: ${NEKO_THREAD_POOL_BUILD_TESTS}")
message(STATUS "  - Neko Thread Pool Enable module: ${NEKO_THREAD_POOL_ENABLE_MODULE}")
message(STATUS "")
message(STATUS "Dependency summary:")
message(STATUS "  - NekoSchema : ${NekoSchema_FOUND} version : ${NekoSchema_VERSION}")
message(STATUS "  - GTest : ${GTest_FOUND} version : ${GTest_VERSION}")
message(STATUS "")

if(NEKO_THREAD_POOL_AUTO_FETCH_DEPS)
    include(FetchContent)

    if (NOT NekoSchema_FOUND)
        message(STATUS "NekoSchema not found; Neko Thread Pool Fetching NekoSchema...")

        FetchContent_Declare(
            NekoSchema
            GIT_REPOSITORY https://github.com/moehoshio/NekoSchema.git
            GIT_TAG        main
        )
        set(NEKO_SCHEMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(NEKO_SCHEMA_ENABLE_MODULE ${NEKO_THREAD_POOL_ENABLE_MODULE} CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(NekoSchema)
    endif()

    if(NOT GTest_FOUND AND NEKO_THREAD_POOL_BUILD_TESTS)
        message(STATUS "GTest not found; Neko Thread Pool Fetching GoogleTest...")

        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.17.0
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

endif(NEKO_THREAD_POOL_AUTO_FETCH_DEPS)


# ================
# = Main target =
# ================

add_library(NekoThreadPool INTERFACE)
add_library(Neko::ThreadPool ALIAS NekoThreadPool)

target_include_directories(NekoThreadPool INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(NekoThreadPool INTERFACE cxx_std_20)

target_link_libraries(NekoThreadPool INTERFACE NekoSchema)

# ================
# = C++20 Module =
# ================

if(NEKO_THREAD_POOL_ENABLE_MODULE)
    message(STATUS "NekoThreadPool C++20 module enabled (NEKO_THREAD_POOL_ENABLE_MODULE=ON)")
    
    # Check CMake version for module support
    if(CMAKE_VERSION VERSION_LESS 3.28)
        message(WARNING "CMake 3.28+ is recommended for full C++20 module support")
    endif()
    
    # Create module library
    add_library(NekoThreadPool_module)
    add_library(Neko::ThreadPool::Module ALIAS NekoThreadPool_module)
    
    target_sources(NekoThreadPool_module
        PUBLIC
            FILE_SET CXX_MODULES FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/include/neko/thread/neko.thread.cppm
    )
    
    target_compile_features(NekoThreadPool_module PUBLIC cxx_std_20)
    
    # Link dependencies (needed for module compilation)
    target_link_libraries(NekoThreadPool_module PUBLIC NekoSchema_module)
    
    # Compiler-specific module flags
    if(MSVC)
        target_compile_options(NekoThreadPool_module PUBLIC /experimental:module)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(NekoThreadPool_module PUBLIC -fmodules-ts)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(NekoThreadPool_module PUBLIC -fmodules)
    endif()
    
    # Module still needs access to the include directory for the headers
    target_include_directories(NekoThreadPool_module PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    
else()
    message(STATUS "NekoThreadPool C++20 module disabled (NEKO_THREAD_POOL_ENABLE_MODULE=OFF)")
endif()

# ================
# ==== Tests =====
# ================

if(NEKO_THREAD_POOL_BUILD_TESTS)
    enable_testing()
    message(STATUS "NekoThreadPool tests enabled (NEKO_THREAD_POOL_BUILD_TESTS=ON)")

    if (NOT Gtest_FOUND AND NOT NEKO_THREAD_POOL_AUTO_FETCH_DEPS)
        message(WARNING "GTest is required for building tests but was not found.")
        message(FATAL_ERROR "Please enable -DNEKO_THREAD_POOL_AUTO_FETCH_DEPS=ON or install GTest and make it discoverable by CMake. e.g -DCMAKE_PREFIX_PATH=</path/to/googletest>")
    endif()

    add_executable(threadPool_test tests/threadPool_test.cpp)
    target_include_directories(threadPool_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(threadPool_test PRIVATE NekoThreadPool NekoSchema GTest::gtest GTest::gtest_main)
    target_compile_features(threadPool_test PRIVATE cxx_std_20)

    include(GoogleTest)
    gtest_discover_tests(threadPool_test)

    # Module-based tests (if module is enabled)
    if(NEKO_THREAD_POOL_ENABLE_MODULE)
        message(STATUS "Neko Thread Pool module tests enabled")
        add_executable(threadPool_module_test tests/threadPool_module_test.cpp)
        target_link_libraries(threadPool_module_test PRIVATE NekoSchema_module NekoThreadPool_module GTest::gtest GTest::gtest_main)
        target_compile_features(threadPool_module_test PRIVATE cxx_std_20)
        gtest_discover_tests(threadPool_module_test)
    endif()
    
else()
    message(STATUS "NekoThreadPool tests disabled (NEKO_THREAD_POOL_BUILD_TESTS=OFF)")
endif()


# ================
# == Install =====
# ================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING 
    PATTERN "*.hpp"
    PATTERN "*.cppm"
)

# Install targets
install(TARGETS NekoThreadPool
    EXPORT NekoThreadPoolTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install export targets
install(EXPORT NekoThreadPoolTargets
    FILE NekoThreadPoolTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NekoThreadPool
)

# Create and install Config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/NekoThreadPoolConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NekoThreadPoolConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NekoThreadPool
)

# Create and install Version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/NekoThreadPoolConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/NekoThreadPoolConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/NekoThreadPoolConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NekoThreadPool
)

message(STATUS "Neko Thread Pool End of configuration")