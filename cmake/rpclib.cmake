
set(rpclib_source_dir ${thirdparty_source}/rpclib)

FetchContent_Declare(
  rpclib
  SOURCE_DIR ${rpclib_source_dir}
  OVERRIDE_FIND_PACKAGE
  SYSTEM
)

block()
  FetchContent_MakeAvailable(rpclib)
endblock()
set(rpclib_INCLUDE_DIR ${rpclib_source_dir})