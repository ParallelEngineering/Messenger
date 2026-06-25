cmake_minimum_required(VERSION 3.28.3)
project(Messenger)

# Enforce C++20 and disable compiler-specific extensions
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(lib)
add_subdirectory(src)

#target_link_libraries(Messenger PRIVATE RSA)