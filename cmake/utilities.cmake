
# Print C and CXX compilation flags with location of the calling function
macro(print_c_cxx_flags)

  message(STATUS "At ${CMAKE_CURRENT_LIST_FILE} status is :")

  message(STATUS "CMAKE_SYSTEM_PROCESSOR : ${CMAKE_SYSTEM_PROCESSOR}")

  # SYSROOT
  if(NOT "${CMAKE_SYSROOT}" STREQUAL "")
    message(STATUS "CMAKE_SYSROOT : ${CMAKE_SYSROOT}")
  else()
    message(STATUS "CMAKE_SYSROOT is empty.")    
  endif()

  message(STATUS "CMAKE_C_LINK_EXECUTABLE : ${CMAKE_C_LINK_EXECUTABLE}")
  message(STATUS "CMAKE_C_LINKER_LAUNCHER : ${CMAKE_C_LINKER_LAUNCHER}")
  message(STATUS "CMAKE_CXX_LINK_EXECUTABLE : ${CMAKE_CXX_LINK_EXECUTABLE}")
  message(STATUS "CMAKE_CXX_LINK_LAUNCHER : ${CMAKE_CXX_LINKER_LAUNCHER}")
  
  # C
  if(NOT "${CMAKE_C_STANDARD}" STREQUAL "")
    message(STATUS "CMAKE_C_STANDARD : ${CMAKE_C_STANDARD}")
  else()
    message(STATUS "CMAKE_C_STANDARD is empty.")    
  endif()

  if(NOT "${CMAKE_C_FLAGS}" STREQUAL "")
    message(STATUS "CMAKE_C_FLAGS : ${CMAKE_C_FLAGS}")
  else()
    message(STATUS "CMAKE_C_FLAGS is empty.")    
  endif()

  if(NOT "${CMAKE_C_FLAGS_DEBUG}" STREQUAL "")
    message(STATUS "CMAKE_C_FLAGS_DEBUG : ${CMAKE_C_FLAGS_DEBUG}")
  else()
    message(STATUS "CMAKE_C_FLAGS_DEBUG is empty.")    
  endif()

  if(NOT "${CMAKE_C_FLAGS_RELEASE}" STREQUAL "")
    message(STATUS "CMAKE_C_FLAGS_RELEASE : ${CMAKE_C_FLAGS_RELEASE}")
  else()
    message(STATUS "CMAKE_C_FLAGS_RELEASE is empty.")    
  endif()

  # CXX 
  if(NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
    message(STATUS "CMAKE_CXX_STANDARD : ${CMAKE_CXX_STANDARD}")
  else()
    message(STATUS "CMAKE_CXX_STANDARD is empty.")    
  endif()

  if(NOT "${CMAKE_CXX_FLAGS}" STREQUAL "")
    message(STATUS "CMAKE_CXX_FLAGS : ${CMAKE_CXX_FLAGS}")
  else()
    message(STATUS "CMAKE_CXX_FLAGS is empty.")    
  endif()

  if(NOT "${CMAKE_CXX_FLAGS_DEBUG}" STREQUAL "")
    message(STATUS "CMAKE_CXX_FLAGS_DEBUG : ${CMAKE_CXX_FLAGS_DEBUG}")
  else()
    message(STATUS "CMAKE_CXX_FLAGS_DEBUG is empty.")    
  endif()

  if(NOT "${CMAKE_CXX_FLAGS_RELEASE}" STREQUAL "")
    message(STATUS "CMAKE_CXX_FLAGS_RELEASE : ${CMAKE_CXX_FLAGS_RELEASE}")
  else()
    message(STATUS "CMAKE_CXX_FLAGS_RELEASE is empty.")    
  endif()

  # EXE LINKER
  if(NOT "${CMAKE_EXE_LINKER_FLAGS}" STREQUAL "")
    message(STATUS "CMAKE_EXE_LINKER_FLAGS : ${CMAKE_EXE_LINKER_FLAGS}")
  else()
    message(STATUS "CMAKE_EXE_LINKER_FLAGS is empty.")    
  endif()

  if(NOT "${CMAKE_EXE_LINKER_FLAGS_DEBUG}" STREQUAL "")
    message(STATUS "CMAKE_EXE_LINKER_FLAGS_DEBUG : ${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
  else()
    message(STATUS "CMAKE_EXE_LINKER_FLAGS_DEBUG is empty.")    
  endif()

  if(NOT "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" STREQUAL "")
    message(STATUS "CMAKE_EXE_LINKER_FLAGS_RELEASE : ${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
  else()
    message(STATUS "CMAKE_EXE_LINKER_FLAGS_RELEASE is empty.")    
  endif()
  
  # STATIC LINKER
  if(NOT "${CMAKE_STATIC_LINKER_FLAGS}" STREQUAL "")
    message(STATUS "CMAKE_STATIC_LINKER_FLAGS : ${CMAKE_STATIC_LINKER_FLAGS}")
  else()
    message(STATUS "CMAKE_STATIC_LINKER_FLAGS is empty.")    
  endif()

  if(NOT "${CMAKE_STATIC_LINKER_FLAGS_DEBUG}" STREQUAL "")
    message(STATUS "CMAKE_STATIC_LINKER_FLAGS_DEBUG : ${CMAKE_STATIC_LINKER_FLAGS_DEBUG}")
  else()
    message(STATUS "CMAKE_STATIC_LINKER_FLAGS_DEBUG is empty.")    
  endif()

  if(NOT "${CMAKE_STATIC_LINKER_FLAGS_RELEASE}" STREQUAL "")
    message(STATUS "CMAKE_STATIC_LINKER_FLAGS_RELEASE : ${CMAKE_STATIC_LINKER_FLAGS_RELEASE}")
  else()
    message(STATUS "CMAKE_STATIC_LINKER_FLAGS_RELEASE is empty.")    
  endif()

  # SHARED LINKER
  if(NOT "${CMAKE_SHARED_LINKER_FLAGS}" STREQUAL "")
    message(STATUS "CMAKE_SHARED_LINKER_FLAGS : ${CMAKE_SHARED_LINKER_FLAGS}")
  else()
    message(STATUS "CMAKE_SHARED_LINKER_FLAGS is empty.")    
  endif()

  if(NOT "${CMAKE_SHARED_LINKER_FLAGS_DEBUG}" STREQUAL "")
    message(STATUS "CMAKE_SHARED_LINKER_FLAGS_DEBUG : ${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
  else()
    message(STATUS "CMAKE_SHARED_LINKER_FLAGS_DEBUG is empty.")    
  endif()

  if(NOT "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}" STREQUAL "")
    message(STATUS "CMAKE_SHARED_LINKER_FLAGS_RELEASE : ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
  else()
    message(STATUS "CMAKE_SHARED_LINKER_FLAGS_RELEASE is empty.")    
  endif()

  # MODULE PATH
  if(NOT "${CMAKE_MODULE_PATH}" STREQUAL "")
    message(STATUS "CMAKE_MODULE_PATH : ${CMAKE_MODULE_PATH}")
  else()
    message(STATUS "CMAKE_MODULE_PATH is empty.")    
  endif()
  
endmacro()

# Print all Targets
function(get_all_targets var)
  set(targets)
  get_all_targets_recursive(targets ${CMAKE_SOURCE_DIR})
  set(${var} ${targets} PARENT_SCOPE)
endfunction()

macro(get_all_targets_recursive targets dir)
  get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
  foreach(subdir ${subdirectories})
    get_all_targets_recursive(${targets} ${subdir})
  endforeach()
  get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
  list(APPEND ${targets} ${current_targets})
endmacro()

function(print_all_targets)
  get_all_targets(all_targets)
  print_beautiful_list("All available targets" "${all_targets}")
endfunction()

# Print a list beautifully, instead of CMake ways.
function(print_beautiful_list __description_of_list__ __ugly_list__)
  list(JOIN __ugly_list__ ", " __beautiful_list__)  
  message(STATUS "${__description_of_list__} :")
  message(STATUS "${__beautiful_list__}")
endfunction()


# Print all variables
function(print_all_variables)
  print_all_variables_with_name("")
endfunction()

# Print all variables that names contain str
function(print_all_variables_with_name str)
  get_cmake_property(_variableNames VARIABLES)
  list (SORT _variableNames)
  foreach (_variableName ${_variableNames})
    if(str STREQUAL "")
        message(STATUS "${_variableName}=${${_variableName}}")
    else()
      if(${_variableName} MATCHES "${str}*")
        message(STATUS "${_variableName}=${${_variableName}}")
      endif()
    endif()
  endforeach()  
endfunction()

# This functions requires that you have initialied the Git package
function(set_submodule_with_specific_commit submodule_path commit_hash)
  if(GIT_FOUND)
    message(STATUS "Setting ${submodule_path} to ${commit_hash}")
    execute_process(COMMAND ${GIT_EXECUTABLE} checkout ${commit_hash}
      WORKING_DIRECTORY ${submodule_path}
      RESULT_VARIABLE GIT_SUBMOD_RESULT)
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
      message(FATAL_ERROR "git ${submodule_path} submodule checkout for ${commit_hash} failed with ${GIT_SUBMOD_RESULT}, please checkout submodules.")
    endif()
    message(STATUS "${submodule_path} submodule setup was successful.")
  else()
    message(FATAL_ERROR "The GIT_FOUND variable is not initialized. This usually means you haven't used find_package(Git).")
  endif()
endfunction()
