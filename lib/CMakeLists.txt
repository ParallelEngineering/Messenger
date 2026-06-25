if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/RSA/CMakeLists.txt")
  message(FATAL_ERROR "RSA submodule not initialized. Run: git submodule update --init --recursive")
endif()
add_subdirectory(RSA)