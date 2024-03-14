
set(googletest_source_dir ${thirdparty_source}/googletest)

FetchContent_Declare(
  googletest
  SOURCE_DIR ${googletest_source_dir}
  OVERRIDE_FIND_PACKAGE
)

block()
  set(BUILD_SHARED_LIBS OFF)
  set(BUILD_TESTING OFF)
  set(INSTALL_GTEST OFF)
  FetchContent_MakeAvailable(googletest)
endblock()
#set(googletest_INCLUDE_DIR ${googletest_SOURCE_DIR}/googletest/include ${googletest_SOURCE_DIR}/googlemock/include)  
