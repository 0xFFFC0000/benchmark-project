
set(cpr_source_dir ${thirdparty_source}/cpr)

FetchContent_Declare(
  cpr
  SOURCE_DIR ${cpr_source_dir}
  OVERRIDE_FIND_PACKAGE
  SYSTEM
)

block()
  FetchContent_MakeAvailable(cpr)
endblock()
set(cpr_INCLUDE_DIR ${cpr_source_dir})