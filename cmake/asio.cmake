
set(asio_source_dir ${thirdparty_source}/asio)

FetchContent_Declare(
  asio
  SOURCE_DIR ${asio_source_dir}
  OVERRIDE_FIND_PACKAGE
  SYSTEM
)

block()
  FetchContent_MakeAvailable(asio)
endblock()
set(asio_INCLUDE_DIR ${asio_source_dir})
