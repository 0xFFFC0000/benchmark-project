
set(benchmark_source_dir ${thirdparty_source}/benchmark)

FetchContent_Declare(
  benchmark
  SOURCE_DIR ${benchmark_source_dir}
  OVERRIDE_FIND_PACKAGE
)

block()
  set(BUILD_SHARED_LIBS OFF)
  set(BUILD_TESTING OFF)
  set(GOOGLETEST_PATH ${googletest_source_dir})
  set(BENCHMARK_ENABLE_TESTING OFF)
  FetchContent_MakeAvailable(benchmark)
endblock()
#set(benchmark_INCLUDE_DIR ${benchmark_SOURCE_DIR}/benchmark/include)
