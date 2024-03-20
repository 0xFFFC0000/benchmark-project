
################################################
cmake_minimum_required(VERSION 3.19)
project(storage-engine LANGUAGES CXX)

################################################
# CMake utilities
include(cmake/utilities.cmake)

# Universal settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(benchmark_project LANGUAGES CXX C)

include(FetchContent)

################################################
# Define global variables
set(thirdparty_source ${CMAKE_SOURCE_DIR}/thirdparty CACHE INTERNAL "Thirdparty source location." FORCE)

if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_definitions(-DDEBUGBUILD)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3 -ggdb")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -ggdb")
endif()

################################################
# Add thirdparty libaries
# Define submodules required commit
set(googletest_dir "googletest")
set(benchmark_dir "benchmark")
set(asio_dir "asio")
set(rpclib_dir "rpclib")

# v1.14.x
set(googletest_REQUIRED_COMMIT "f8d7d77c06936315286eb55f8de22cd23c188571")
# v1.8.3
set(benchmark_REQUIRED_COMMIT "344117638c8ff7e239044fd0fa7085839fc03021")
# boost-1.83.0
set(asio_REQUIRED_COMMIT "f2fbbd824c1fafa67c1b9c7ee6e4d2649b343a46")
# cpr 1.10.5
set(cpr_REQUIRED_COMMIT "3b15fa82ea74739b574d705fea44959b58142eb8")
# rpclib v2.3.0
set(rpclib_REQUIRED_COMMIT "a663a1598a4b419123b2e13c0ae6a39c91dcf5b8")

################################################
# Git submodules setup
find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
  set_submodule_with_specific_commit(${thirdparty_source}/${googletest_dir}      ${googletest_REQUIRED_COMMIT})
  set_submodule_with_specific_commit(${thirdparty_source}/${benchmark_dir}       ${benchmark_REQUIRED_COMMIT})
  set_submodule_with_specific_commit(${thirdparty_source}/${asio_dir}            ${asio_REQUIRED_COMMIT})
  set_submodule_with_specific_commit(${thirdparty_source}/${rpclib_dir}          ${rpclib_REQUIRED_COMMIT})
else()
  message(FATAL_ERROR "Cannot find git executable. Make sure you have installed git.")
endif()

################################################
# googltest only if testing is enabled.
if(BUILD_TESTS)
  include(cmake/googletest.cmake)
endif()

################################################
# googlebenchmark
include(cmake/benchmark.cmake)

################################################
# asio
include(cmake/asio.cmake)

################################################
# cpr
include(cmake/cpr.cmake)

################################################
# rpclib
include(cmake/rpclib.cmake)

################################################
# Boost
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.81.0 COMPONENTS system filesystem thread date_time chrono regex serialization program_options log json)

if(NOT Boost_FOUND)
  message(FATAL_ERROR "Cannot find boost libraries")
endif()

add_executable(benchmark_project benchmark_project.cpp)
target_include_directories(benchmark_project PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(benchmark_project PRIVATE gtest ${Boost_LIBRARIES} cpr::cpr rpc)
