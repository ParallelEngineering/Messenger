find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(Messenger-Shared STATIC
        message.cpp
        message.h
)

target_link_libraries(Messenger-Shared
        PUBLIC
            Qt6::Core
)

target_include_directories(Messenger-Shared
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
)
